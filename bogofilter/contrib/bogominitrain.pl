#!/usr/bin/perl
# Script to train bogofilter from mboxes
# by Boris 'pi' Piwinger <3.14@piology.org>

# Not correct number of parameters
unless (scalar(@ARGV)==3 || (scalar(@ARGV)==4 && $ARGV[0] =~ /^-(?=[vf]{1,3}$)(?:f?v{0,2}|v+f)$/)) {
  print <<END unless (scalar(@ARGV)==3 || (scalar(@ARGV)==4 && $ARGV[0] eq "-v"));

Usage:
  bogominitrain.pl [-[f][v[v]]] <database-directory> <ham-mboxes> <spam-mboxes>

  Uses a "train on error" process to build minimal wordlists that can correctly 
  score all messages.

  Run "formail -es" on your mailboxes before you start to ensure their
  correctness.

  It may be a good idea to run this script command several times (until no
  scoring errors occur).  Use the '-f' option to do this.

Example:
  bogominitrain.pl .bogofilter 'ham*' 'spam*'

Options:
  -v   This switch produces info on messages used for training.
  -vv  In addition messages not used for training are listed.
  -f   Runs the program until no error remains.
  Note: If you need to use more than one option, you must combine them.

Warning:
  If you've changed the classification of a message, bogofilter's ability to
  score messages may be affected adversely.  To prevent this, you should train
  again with your complete mail database to correct the problem.

END
  exit;
}

# Check input and open
my $force=1 if (scalar(@ARGV)==4 && $ARGV[0]=~s/f//);
my $verbose=1 if (scalar(@ARGV)==4 && $ARGV[0]=~s/^-v/-/);
my $vverbose=1 if (scalar(@ARGV)==4 && $ARGV[0] eq "-v");
shift (@ARGV) if (scalar(@ARGV)==4);
my ($dir,$ham,$spam) = @ARGV;
my $temp; srand; do {$temp="/tmp/".rand();} until (!-e $temp);

die ("$dir is not a directory or not accessible.\n") unless (-d $dir && -r $dir && -w $dir && -x $dir);
print "\nStarting with this database:\n";
my $dbexists=(-s "$dir/goodlist.db");
if ($dbexists) {print `bogoutil -w $dir .MSG_COUNT`;} else {print "  (empty)\n";}

my ($fp,$fn);
do { # Start force loop
  open (HAM, "cat $ham|")   || die("Cannot open ham: $!\n");
  open (SPAM, "cat $spam|") || die("Cannot open spam: $!\n");

  # Loop through all the mails
  # bogofilter return codes: 0 for spam; 1 for non-spam
  my ($lasthamline,$lastspamline,$hamcount,$spamcount,$hamadd,$spamadd) = ("","",0,0,0,0);
  do {

    # Read one mail from ham box and test, train as needed
    unless (eof(HAM)) {
      my $mail=$lasthamline;
      $lasthamline="";
      while (defined(my $line=<HAM>)) {
	if ($line=~/^From /) {$lasthamline=$line; last;}
	$mail.=$line;
      }
      if ($mail) {
	$hamcount++;
	open (TEMP, ">$temp") || die "Cannot write to temp file: $!";
	print TEMP $mail;
	close (TEMP);
	unless ($dbexists && system("bogofilter -d $dir <$temp")/256==1) {
	  system("bogofilter -d $dir -n <$temp");
	  $hamadd++;
	  $dbexists=1;
	  print "Training ham message $hamcount.\n" if ($verbose);
	} else {print "Not training ham message $hamcount..\n" if ($vverbose);}
	unlink ($temp);
      }
    }

    # Read one mail from spam box and test, train as needed
    unless (eof(SPAM)) {
      my $mail=$lastspamline;
      $lastspamline="";
      while (!eof(SPAM) && defined(my $line=<SPAM>)) {
	if ($line=~/^From /) {$lastspamline=$line; last;}
	$mail.=$line;
      }
      if ($mail) {
	$spamcount++;
	open (TEMP, ">$temp") || die "Cannot write to temp file: $!";
	print TEMP $mail;
	close (TEMP);
	unless (system("bogofilter -d $dir <$temp")/256==0) {
	  system("bogofilter -d $dir -s <$temp");
	  $spamadd++;
	  print "Training spam message $spamcount.\n" if ($verbose);
	} else {print "Not training spam message $spamcount.\n" if ($vverbose);}
	unlink ($temp);
      }
    }

  } until (eof(HAM) && eof(SPAM));
  close (HAM);
  close (SPAM);

  print "\nDone:\n";
  print "Read $hamcount ham mails and $spamcount spam mails.\n";
  print "Added $hamadd ham mails and $spamadd spam mails to the database.\n";
  print `bogoutil -w $dir .MSG_COUNT`;
  $fn=`cat $spam | bogofilter -d $dir -vM | grep -c Ham`;
  print "\nFalse negatives: $fn";
  $fp=`cat $ham | bogofilter -d $dir -vM | grep -c Spam`;
  print "False positives: $fp\n";
} until ($fn+$fp==0 || !$force)
