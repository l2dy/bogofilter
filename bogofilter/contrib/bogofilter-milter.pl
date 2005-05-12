#!/usr/bin/perl

# $Id$
#
# bogofilter-milter.pl - a Sendmail::Milter Perl script for filtering
# mail using individual users' bogofilter databases.

# Copyright 2003, 2005 Jonathan Kamens <jik@kamens.brookline.ma.us>.
# Please send me bug reports, suggestions, criticisms, compliments, or
# any other feedback you have about this script!
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version. 
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details. 

# You will need the following non-standard Perl modules installed to
# use this script: Sendmail::Milter, Mail::Alias, Proc::Daemon,
# IO::Stringy.  Before using this script, search for CONFIGURABLE
# SETTINGS and configure them appropriately for your site.
#
# Inserts "X-Bogosity: Spam, tests=bogofilter" into messages that
# appear to be spam (or "Ham" into ones that don't).  If the message is
# rejected, you usually won't see the "Spam", but see below about
# training mode.
#
# Save this script somewhere, launch it as root (by running it in the
# background or invoking it with "--daemon" in which case it will
# background itself), and reconfigure your sendmail installation to
# call it as an external filter (probably by calling INPUT_MAIL_FILTER
# in your sendmail.mc file).  Running this script as root should be
# safe because it changes its effective UID and GID whenever
# performing operations on individual users' files (if you find a
# security problem, please let me know!)
#
# For additional information about libmilter and integrating this or
# any other libmilter filter into your sendmail installation, see the
# file README.libmilter that ships with sendmail and/or the section
# entitled "ADDING NEW MAIL FILTERS" in the README file that ships
# with the M4 sendmail CF templates.
#
# You may need to restart this script to get it to notice changes in
# mail aliases.

# This script logs various informational, warning and error messages
# to the "mail" facility.

# BEGIN CONFIGURABLE SETTINGS

# If this string appears in the Subject of a message (case
# insensitive), the message won't be filtered.
my $magic_string = '[no-bogofilter]';

# The largest message to keep in memory rather than writing to a
# temporary file.
my $MAX_INCORE_MSG_LENGTH = 1000000;

my $pid_file = '/var/run/bogofilter-milter.pid';

# Whatever path you specify for $socket needs to match the socket
# specified in the sendmail.cf file (with "local:" in front of it
# there, but not here).
my $socket = '/var/run/bogofilter-milter.sock';

# If a file with this name exists in the user's .bogofilter directory,
# then that user's mail will be filtered in training mode.  This means
# that the message will be filtered and registered as spam or non-spam
# and the appropriate X-Bogosity header will be inserted, but it'll be
# delivered even if bogofilter thinks it's spam.  This allows the user
# to detect false positives or false negatives and feed them back into
# bogofilter to train it.  To disable this functionality set
# $training_file to undef.
my $training_file = 'training';

# If a file or link with this name exists in the user's .bogofilter
# directory, then copies of rejected messages will be saved in this
# file in mbox format, using flock locking.  To disable rejected
# message archiving, set $archive_mbox to undef.
my $archive_mbox = 'archive';

# Mail::Alias is used to expand SMTP recipient addresses into local
# mailboxes to determine if any of them have bogofilter databases.  If
# someone sends E-mail to a mailing list or alias whose expansion
# contains one or more local users with bogofilter databases, then one
# of those users' database (which one in particular is not defined)
# will be used to filter the message.  To disable this functionality
# and remove the dependency on Mail::Alias, comment out the "use
# Mail::Alias;" line and set $aliases_file to undef in the
# configuration section.  With this functionality disabled, mail will
# only be filtered if it is sent directly to a user in the passwd
# file.  On the other hand, with this functionality enabled, one
# person's bogofilter database can cause a message to be filtered for
# everyone on a local mailing list.
my $aliases_file = '/etc/aliases';

# You may wish to remove this restriction, by setting this variable to
# 0, if your site gets a lot of mail, but I haven't tested the script
# to make sure it functions correctly with multiple interpreters.
my $milter_interpreters = 1;

# END CONFIGURABLE SETTINGS

require 5.008_000; # for User::pwent

use strict;
use warnings;
use Sendmail::Milter;
use File::Basename;
use File::Temp qw(tempfile);
use Fcntl qw(:flock :seek);
use Mail::Alias;
use User::pwent;
use Sys::Syslog qw(:DEFAULT setlogsock);
use English '-no_match_vars';
use Proc::Daemon;
use Getopt::Long;
use IO::Scalar;

# Used to cache the results of alias expansions and checks for
# filtered recipients.
my %cached_recipients;

my $whoami = basename $0;
my $usage = "Usage: $whoami [--daemon] [--debug]\n";
my($run_as_daemon, $get_help, $debug);

my %my_milter_callbacks =
(
 'envrcpt' => \&my_rcpt_callback,
 'header'  => \&my_header_callback,
 'eoh'     => \&my_eoh_callback,
 'body'    => \&my_body_callback,
 'eom'     => \&my_eom_callback,
 'abort'   => \&my_abort_callback,
 );

dia $usage if (! GetOptions('daemon' => \$run_as_daemon,
			    'debug' => \$debug,
			    'help|h|?' => \$get_help));
if ($get_help) {
    print $usage;
    exit;
}

if ($run_as_daemon) {
    Proc::Daemon::Init;
}

my $magic_string_re = $magic_string;
$magic_string_re =~ s/(\W)/\\$1/g;

setlogsock('unix');
openlog($whoami, 'pid', 'mail');

if (! (open(PIDFILE, '+<', $pid_file) ||
       open(PIDFILE, '+>', $pid_file))) {
    &die("open($pid_file): $!\n");
}

seek(PIDFILE, 0, SEEK_SET);

if (! flock(PIDFILE, LOCK_EX|LOCK_NB)) {
    &die("flock($pid_file): $!\n");
}
if (! (print(PIDFILE "$$\n"))) {
    &die("writing to $pid_file: $!\n");
}
# Flush the PID
seek(PIDFILE, 0, SEEK_SET);

unlink($socket);
Sendmail::Milter::setconn("local:$socket");
Sendmail::Milter::register("bogofilter-milter",
			   \%my_milter_callbacks, SMFI_CURR_ACTS);

Sendmail::Milter::main($milter_interpreters);

sub my_rcpt_callback {
    my $ctx = shift;
    my $hash = $ctx->getpriv();

    if ($hash->{'rcpt'}) {
	# We've already encountered a recipient who is filtering this message.
	$ctx->setpriv($hash);
	return SMFIS_CONTINUE;
    }
    my $rcpt = $ctx->getsymval('{rcpt_addr}');

    if (&filtered_dir($rcpt)) {
	$hash->{'rcpt'} = $rcpt;
	$ctx->setpriv($hash);
	return SMFIS_CONTINUE;
    }
    else {
	$ctx->setpriv(undef);
	return SMFIS_CONTINUE;
    }
}

sub my_header_callback {
    my($ctx, $field, $value) = @_;
    my($hash) = $ctx->getpriv();

    return SMFIS_ACCEPT if (! $hash);

    if (($field =~ /^subject$/i) && ($value =~ /$magic_string_re/oi)) {
	$ctx->setpriv(undef);
	return SMFIS_ACCEPT;
    }

    $hash = &add_to_message($hash, "$field: $value\n");

    $ctx->setpriv($hash);

    return SMFIS_CONTINUE;
}

sub my_eoh_callback {
    my($ctx) = @_;
    my($hash) = $ctx->getpriv();

    $hash = &add_to_message($hash, "\n");

    $ctx->setpriv($hash);

    return SMFIS_CONTINUE;
}

sub my_body_callback {
    my($ctx, $body, $len) = @_;
    my($hash) = $ctx->getpriv();

    $hash = &add_to_message($hash, $body);

    $ctx->setpriv($hash);

    return SMFIS_CONTINUE;
}

sub add_to_message {
    my($hash, $text) = @_;
    return $hash if (! $text);

    if (! $hash->{'fh'}) {
	$hash->{'msg'} = '' if (! $hash->{'msg'});
	$hash->{'msg'} .= $text;

	if (length($hash->{'msg'}) <= $MAX_INCORE_MSG_LENGTH) {
	    return $hash;
	}

	($hash->{'fh'}, $hash->{'fn'}) = tempfile();

	if (! $hash->{'fn'}) {
	    &die("error creating temporary file");
	}

	syslog('debug', '%s', "switching to temporary file " . $hash->{'fn'});

	$text = $hash->{'msg'};
	delete $hash->{'msg'};
    }

    if (! print({$hash->{'fh'} } $text)) {
	&die("error writing to temporary file " . $hash->{'fn'});
    }

    return $hash;
}

sub message_read_handle {
    my($hash) = @_;

    if ($hash->{'fn'}) {
	if (! seek($hash->{'fh'}, 0, SEEK_SET)) {
	    &die("couldn't seek in " . $hash->{'fn'} . ": $!");
	}
	return $hash->{'fh'};
    }
    else {
	return new IO::Scalar \$hash->{'msg'};
    }
}

    
sub my_eom_callback {
    my $ctx = shift;
    my $hash = $ctx->getpriv();
    my $fh;
    local($_);

    my $dir = &filtered_dir($hash->{'rcpt'});

    if (! $dir) {
	&die("my_eom_callback called for non-filtered recipient " . $hash->{'rcpt'} . "\n");
    }

    my $pid = open(BOGOFILTER, '|-');
    if (! defined($pid)) {
	&die("opening bogofilter: $!\n");
    }
    elsif (! $pid) {
	&die("couldn't restrict permissions") if
	    (! &restrict_permissions($hash->{'rcpt'}, 1));;
	exec('bogofilter', '-u', '-d', $dir) ||
	    &die("exec(bogofilter): $!\n");
	# &die had better not return!
    }

    $fh = &message_read_handle($hash);
    if ($hash->{'fn'}) {
	# This is safe to do on Unix, since on Unix you can unlink an
	# open file and it'll stay around until the last open file
	# handle to it goes away.  If this script were to be used on
	# non-Unix operating systems, which is a big "if" that I'm not
	# sure could ever happen, then this unlink might be a problem
	# and would need to happen later.
	unlink $hash->{'fn'};
    }

    while (<$fh>) {
	s/\r\n$/\n/;
	print(BOGOFILTER $_) || &die("writing to bogofilter: $!\n");
    }

    if (close(BOGOFILTER)) {
	my($training);
	if ($training_file) {
	    if (&restrict_permissions($hash->{'rcpt'})) {
		$training = (-f "$dir/$training_file");
		&unrestrict_permissions;
	    }
	    else {
		&syslog('warning', 'assuming training mode because ' .
			'permissions could not be restricted');
		$training = 1;
	    }
	}
	$ctx->addheader('X-Bogosity', 'Spam, tests=bogofilter');
	my $from = $ctx->getsymval('{mail_addr}');
	syslog('info', '%s', ($training ? "would reject" : "rejecting") . 
	       " likely spam from $from to " . $hash->{'rcpt'} . " based on $dir");
	if (! $training) {
	    my $archive;

	    $archive = ($archive_mbox &&
			&restrict_permissions($hash->{'rcpt'}) &&
			(lstat($archive = "$dir/$archive_mbox"))) ?
			$archive : undef;

	    if ($archive) {
		# There is an annoying race condition here.  Suppose two spam
		# messages are delivered at the same time to a user whose
		# archive file is a symlink pointing at a nonexistent (yet)
		# file.  Milter process A tries to open with +< and fails.  IN
		# the meantime, process B also tries to open with +< and fails.
		# Then A opens witn +>, locks the file and starts writing to
		# it, and *then* B opens with +>, thus truncating whatever data
		# was written thus far by A.  I'm not sure what the best way is
		# to fix this race condition reliably, and it seems rare enough
		# that it isn't worth the effort.
		if (! (open(MBOX, '+<', $archive) ||
		       open(MBOX, '+>', $archive))) {
		    syslog('warning', '%s', "opening $archive for " .
			   "write: $!");
		    goto no_archive_open;
		}
		if (! flock(MBOX, LOCK_EX)) {
		    syslog('warning', '%s', "locking $archive: $!");
		    goto close_archive;
		}
		if (! seek(MBOX, 0, SEEK_END)) {
		    syslog('warning', '%s', 
			   "seek($archive, 0, SEEK_END): $!");
		    goto close_archive;
		}
		if (! seek($fh, 0, SEEK_SET)) {
		    &die("error rewinding message handle: $!");
		}

		if (! print(MBOX "From " . ($from || 'MAILER-DAEMON') .
			    "  " . localtime() . "\n")) {
		    syslog('warning', '%s', "write($archive): $!");
		    goto close_archive;
		}

		my($last_blank, $last_nl);

		while (<$fh>) {
		    s/\r\n/\n/;
		    if (! print(MBOX $_)) {
			syslog('warning', '%s', "write($archive): $!");
			goto close_archive;
		    }

		    $last_nl = ($_ =~ /\n/);
		    $last_blank = ($_ eq "\n");
		}

		# Mbox format requires a blank line at the end
		if (! ($last_blank || print(MBOX ($last_nl ? "\n" : "\n\n")))) {
		    syslog('warning', '%s', "write($archive): $!");
		    goto close_archive;
		}

	      close_archive:
		if (! close(MBOX)) {
		    syslog('warning', '%s', "close($archive): $!");
		}
	    }
	  no_archive_open:
	    &unrestrict_permissions;
	    $ctx->setreply(550, "5.7.1", "Your message looks like spam.\n" .
			   "If it isn't, resend it with $magic_string " .
			   "in the Subject line.");
	    $ctx->setpriv(undef);
	    return SMFIS_REJECT;
	}
    }
    else {
	$ctx->addheader('X-Bogosity', 'Ham, tests=bogofilter');
    }

    $ctx->setpriv(undef);
    return SMFIS_CONTINUE;
}

sub my_abort_callback {
    my($ctx) = shift;
    my $hash = $ctx->getpriv();

    if ($hash->{'fn'}) {
	unlink $hash->{'fn'};
    }

    $ctx->setpriv(undef);
    return SMFIS_CONTINUE;
}

sub filtered_dir {
    my($uid, $gid, $dir) = &expand_recipient($_[0]);
    $dir;
}

sub restrict_permissions {
    my($rcpt) = shift;
    my($no_going_back) = shift;

    my($uid, $gid, $dir) = &expand_recipient($rcpt);
    if (! (defined($uid) && defined($gid))) {
	syslog('err', '%s', "internal error: couldn't determine UID and GID " .
	       "for $rcpt");
	return undef;
    }
    $EUID = $uid;
    $EGID = $gid;
    if ($no_going_back) {
	# When we're ready to exec an external program, i.e.,
	# bogofilter, we want to set the real UID and GID so that,
	# e.g., bogofilter will look in the correct home directory for
	# .bogofilter.cf.
	$UID = $uid;
	$GID = $gid;
    }
    1;
}

sub unrestrict_permissions {
    $EUID = $UID;
    $EGID = $GID;
}

sub expand_recipient {
    my($rcpt) = @_;
    my(@expanded);

    if (defined($cached_recipients{$rcpt})) {
	return(@{$cached_recipients{$rcpt}});
    }
    if ($rcpt =~ /\@/) {
	return(@{$cached_recipients{$rcpt}} = ());
    }

    if ($aliases_file) {
	my $aliases = Mail::Alias::Sendmail->new($aliases_file);
	@expanded = $aliases->expand($rcpt);
    }
    else {
	@expanded = ($rcpt);
    }

    if ((@expanded == 1) && ($expanded[0] eq $rcpt)) {
	my($dir, $pw);
	my $stripped = $rcpt;

	$stripped =~ s/\+.*//;
	$pw = getpwnam($stripped);
	@{$cached_recipients{$rcpt}} = $pw ? ($pw->uid, $pw->gid, undef) : ();
	if ($pw && $pw->dir && &restrict_permissions($rcpt) &&
	    -d ($dir = $pw->dir . "/.bogofilter")) {
	    $cached_recipients{$rcpt}->[2] = $dir;
	}
	&unrestrict_permissions;
	return(@{$cached_recipients{$rcpt}});
    }
    else {
	foreach my $addr (@expanded) {
	    my(@sub);
	    if (@sub = &expand_recipient($addr)) {
		return(@{$cached_recipients{$rcpt}} = @sub);
	    }
	}
	return(@{$cached_recipients{$rcpt}} = ());
    }
}

sub die {
    my(@msg) = @_;

    syslog('err', '%s', "@msg");
    exit(1);
}
