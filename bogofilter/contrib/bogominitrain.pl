Return-Path: <3.14@logic.univie.ac.at>
Delivered-To: relson@osagesoftware.com
Received: from server.logic.univie.ac.at (server.logic.univie.ac.at [131.130.190.41])
	by osagesoftware.com (Postfix) with ESMTP id 7B50C27ED5
	for <relson@osagesoftware.com>; Tue, 19 Aug 2003 07:28:26 -0400 (EDT)
Received: from pi.logic.univie.ac.at
	([131.130.190.46] helo=logic.univie.ac.at ident=[+24Mp1ibB5LxMg9Uk00/5LNkHMasd3Ax])
	by server.logic.univie.ac.at with esmtp (Exim 4.20)
	id 19p4f3-0002dk-M5
	for relson@osagesoftware.com; Tue, 19 Aug 2003 13:28:25 +0200
Message-ID: <3F4209D8.10703@logic.univie.ac.at>
Date: Tue, 19 Aug 2003 13:28:24 +0200
From: Boris 'pi' Piwinger <3.14@logic.univie.ac.at>
Organization: Institute for Logic, University of Vienna
User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.4) Gecko/20030618
X-Accept-Language: en, de, de-AT, de-DE
MIME-Version: 1.0
To: David Relson <relson@osagesoftware.com>
Subject: Re: bogominitrain (continued)
References: <4.3.2.7.2.20030817193147.02fb2d90@mail.osagesoftware.com>	<9fr0kvci4b16ht1j4bgvijmkie5r658pke@4ax.com>	<20030818083002.53d5de04.relson@osagesoftware.com>	<hvu1kvkbpcnfe50d4u1ts6pc9fotq33rjv@4ax.com> <20030818171103.34e90431.relson@osagesoftware.com>
In-Reply-To: <20030818171103.34e90431.relson@osagesoftware.com>
Content-Type: multipart/mixed;
 boundary="------------030805050307010708040004"
X-Bogosity: Ham, tests=bogofilter-f, spamicity=0.00e+00, version=0.14.5
Status:   

This is a multi-part message in MIME format.
--------------030805050307010708040004
Content-Type: text/plain; charset=us-ascii
Content-Transfer-Encoding: 7bit

[again corrected, sorry, now my reader fouled me]

David Relson wrote:

> Anyhow, here's the patch (in my usual crude perl style).  Feel free to "do it right".

OK, this is really perlish now. Since I don't know against
which version I should diff, I send you the full version.

pi

--------------030805050307010708040004
Content-Type: text/plain;
 name="bogominitrain.pl.1.32"
Content-Transfer-Encoding: 7bit
Content-Disposition: inline;
 filename="bogominitrain.pl.1.32"

#! /usr/bin/perl
# Script to train bogofilter from mboxes
# by Boris 'pi' Piwinger <3.14@piology.org>
# with many useful additions by David Relson <relson@osagesoftware.com>

# Not correct number of parameters
my $commandlineoptions=($ARGV[0]=~/^-(?=[^f]*f?[^f]*$)(?=[^n]*n?[^n]*$)(?=[^s]*s?[^s]*$)[fns]*v{0,2}[fns]*$/);
unless (scalar(@ARGV)-$commandlineoptions==3 || scalar(@ARGV)-$commandlineoptions==4) {
  print <<END;

bogominitrain.pl version 1.32
  requires bogofilter 0.14.5 or later

Usage:
  bogominitrain.pl [-[f][v[v]][s]] <database-directory> <ham-mboxes>\\ 
    <spam-mboxes> [bogofilter-options]

  database-directory is the directory containing your database files.
  They will be created as needed.  ham-mboxes and spam-mboxes are the
  mboxes containing the mails; the will be shell-expanded.
  bogofilter-options are given to bogofilter literally.

  Uses a "train on error" process to build minimal wordlists that can
  correctly score all messages.

  Run "formail -es" on your mailboxes before you start to ensure their
  correctness.

  It may be a good idea to run this script command several times.  Use
  the '-f' option to run the script until no scoring errors occur.
  The '-n' option will prevent messages from being added more than
  once; this may leave errors in the end.

  To increase the size of your wordlists and to improve bogofilter's
  scoring accuracy, use bogofilter's -o option to set ham_cutoff and
  spam_cutoff to create an "unsure" interval around your normal
  spam_cutoff.  The script will train so that the messages will avoid
  this interval, i.e., all messages in your training mboxes will be
  marked as ham or spam with values far from your production cutoff.
  For example if you usually work with spam_cutoff=0.6, you might use
  the following as bogofilter-options: '-o 0.8,0.3'

Example:
  bogominitrain.pl -fv .bogofilter 'ham*' 'spam*' '-c train.cf'

Options:
  -v   This switch produces info on messages used for training.
  -vv  also lists messages not used for training.
  -f   Runs the program until no errors remain.
  -n   Prevents repetitions.
  -s   Saves the messages used for training to files
       bogominitrain.ham.* and bogominitrain.spam.*
  Note: If you need to use more than one option, you must combine them.

Warning:
  If you've changed the classification of a message, bogofilter's ability to
  score messages may be affected adversely.  To prevent this, you should train
  again with your complete mail database to correct the problem.

END
  exit;
}

# Check input
my $force=1 if ($commandlineoptions && $ARGV[0]=~s/f//);
my $norepetitions=1 if ($commandlineoptions && $ARGV[0]=~s/n//);
my ($safe,$safeham,$safespam)=(1,"bogominitrain.ham","bogominitrain.spam")
   if ($commandlineoptions && $ARGV[0]=~s/s//);
my $verbose=1 if ($commandlineoptions && $ARGV[0]=~s/^-v/-/);
my $vverbose=1 if ($commandlineoptions && $ARGV[0] eq "-v");
shift (@ARGV) if ($commandlineoptions);
my ($dir,$ham,$spam,$options) = @ARGV;
my $bogofilter="bogofilter $options -d $dir";
my $temp; srand; do {$temp="/tmp/".rand();} until (!-e $temp);
die ("$dir is not a directory or not accessible.\n") unless (-d $dir && -r $dir && -w $dir && -x $dir);
`$bogofilter -n < /dev/null` unless (-s "$dir/goodlist.db" || -s "$dir/wordlist.db");
my $ham_total=`cat $ham|grep -c "^From "`;
my $spam_total=`cat $spam|grep -c "^From "`;
my ($fp,$fn,$hamadd,$spamadd,%trainedham,%trainedspam);
my $runs=0;

print "\nStarting with this database:\n";
print `bogoutil -w $dir .MSG_COUNT`,"\n";

do { # Start force loop
  $runs++;
  open (HAM,  "cat $ham|")  || die("Cannot open ham: $!\n");
  open (SPAM, "cat $spam|") || die("Cannot open spam: $!\n");

  # Loop through all the mails
  my ($lasthamline,$lastspamline,$hamcount,$spamcount) = ("","",0,0);
  ($hamadd,$spamadd)=(0,0);
  do {

    # Read one mail from ham box and test, train as needed
    unless (eof(HAM) || $hamcount/$ham_total > $spamcount/$spam_total) {
      my $mail=$lasthamline;
      $lasthamline="";
      while (defined(my $line=<HAM>)) {
        if ($line=~/^From /) {$lasthamline=$line; last;}
        $mail.=$line;
      }
      if ($mail) {
        $hamcount++;
        unless ($norepetitions && $trainedham{$hamcount}) {
          open (TEMP, ">$temp") || die "Cannot write to temp file: $!";
          print TEMP $mail;
          close (TEMP);
          unless ((my $sh=sprintf("%s %8.6f",split(/\s/,`$bogofilter -T <$temp`)))=~/^H/) {
            system("$bogofilter -n <$temp");
            $hamadd++;
            $trainedham{$hamcount}++;
            print "Training  ham message $hamcount",
                  $trainedham{$hamcount}>1&&" ($trainedham{$hamcount})",
                  ": $sh\n" if ($verbose);
            if ($safe) {
              open (TEMP, ">>$safeham.$runs") || die "Cannot write to $safeham.$runs: $!";
              print TEMP $mail;
              close (TEMP);
            }
          } else {print "Not training  ham message $hamcount: $sh\n" if ($vverbose);}
          unlink ($temp);
        }
      }
    }

    # Read one mail from spam box and test, train as needed
    unless (eof(SPAM) || $spamcount/$spam_total > $hamcount/$ham_total) {
      my $mail=$lastspamline;
      $lastspamline="";
      while (!eof(SPAM) && defined(my $line=<SPAM>)) {
        if ($line=~/^From /) {$lastspamline=$line; last;}
        $mail.=$line;
      }
      if ($mail) {
        $spamcount++;
        unless ($norepetitions && $trainedspam{$spamcount}) {
          open (TEMP, ">$temp") || die "Cannot write to temp file: $!";
          print TEMP $mail;
          close (TEMP);
          unless ((my $sh=sprintf("%s %8.6f",split(/\s/,`$bogofilter -T <$temp`)))=~/^S/) {
            system("$bogofilter -s <$temp");
            $spamadd++;
            $trainedspam{$spamcount}++;
            print "Training spam message $spamcount",
                  $trainedspam{$spamcount}>1&&" ($trainedspam{$spamcount})",
                  ": $sh\n" if ($verbose);
            if ($safe) {
              open (TEMP, ">>$safespam.$runs") || die "Cannot write to $safespam.$runs: $!";
              print TEMP $mail;
              close (TEMP);
            }
          } else {print "Not training spam message $spamcount: $sh\n" if ($vverbose);}
          unlink ($temp);
        }
      }
    }

  } until (eof(HAM) && eof(SPAM));
  close (HAM);
  close (SPAM);

  print "\nEnd of run #$runs:\n";
  print "Read $hamcount ham mails and $spamcount spam mails.\n";
  print "Added $hamadd ham mail",$hamadd!=1&&"s"," and $spamadd spam mail",$spamadd!=1&&"s"," to the database.\n";
  print `bogoutil -w $dir .MSG_COUNT`;
  $fn=`cat $spam | $bogofilter -TM | grep -cv ^S`;
  print "\nFalse negatives: $fn";
  $fp=`cat $ham | $bogofilter -TM | grep -cv ^H`;
  print "False positives: $fp\n";
} until ($fn+$fp==0 || $hamadd+$spamadd==0 || !$force);
print "\n$runs run",$runs>1&&"s"," needed to close off.\n" if ($force);

--------------030805050307010708040004--
