#!/usr/bin/perl
# Script to train bogofilter from mboxes
# by Boris 'pi' Piwinger <3.14@piology.org>

# Not correct number of parameters
my $commandlineoptions=($ARGV[0]=~/^-(?=[^v]*v{0,2}[^v]*$)(?=[^f]*f?[^f]*$)(?=[^s]*s?[^s]*$)[vfs]+$/);
unless (scalar(@ARGV)-$commandlineoptions==3 || scalar(@ARGV)-$commandlineoptions==4) {
  print <<END;

bogominitrain.pl version 1.01

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

  To increase the size of your wordlists and to improve bogofilter's
  scoring accuracy, use bogofilter's -o option to set ham_cutoff and
  spam_cutoff to create an "unsure" interval around your normal
  spam_cutoff.  The script will train so that the messages will avoid
  this interval, i.e., all messages in your training mboxes will be
  marked as ham or spam with values far from your production cutoff.
  For example if you usually work with spam_cutoff=0.6, you might use
  the following as bogofilter-options: '-o 0.7,0.5'

Example:
  bogominitrain.pl -fv .bogofilter 'ham*' 'spam*' '-c train.cf'

Options:
  -v   This switch produces info on messages used for training.
  -vv  also lists messages not used for training.
  -f   Runs the program until no errors remain.
  -s   Saves the messages used for training to files
       bogominitrain.ham and bogominitrain.spam
  Note: If you need to use more than one option, you must combine them.

Warning:
  If you've changed the classification of a message, bogofilter's ability to
  score messages may be affected adversely.  To prevent this, you should train
  again with your complete mail database to correct the problem.

END
  exit;
}

# Check input and open
my $force=1 if ($commandlineoptions && $ARGV[0]=~s/f//);
my ($safe,$safeham,$safespam)=(1,"bogominitrain.ham","bogominitrain.spam")
   if ($commandlineoptions && $ARGV[0]=~s/s//);
my $verbose=1 if ($commandlineoptions && $ARGV[0]=~s/^-v/-/);
my $vverbose=1 if ($commandlineoptions && $ARGV[0] eq "-v");
shift (@ARGV) if ($commandlineoptions);
my ($dir,$ham,$spam,$options) = @ARGV;
my $bogofilter="bogofilter $options -d $dir";
my $temp; srand; do {$temp="/tmp/".rand();} until (!-e $temp);

die ("$dir is not a directory or not accessible.\n") unless (-d $dir && -r $dir && -w $dir && -x $dir);
print "\nStarting with this database:\n";
my $dbexists=(-s "$dir/goodlist.db" || -s "$dir/wordlist.db");
if ($dbexists) {print `bogoutil -w $dir .MSG_COUNT`,"\n";} else {print "  (empty)\n\n";}

my ($fp,$fn);
my $runs=0;
do { # Start force loop
  $runs++;
  open (HAM, "cat $ham|")   || die("Cannot open ham: $!\n");
  open (SPAM, "cat $spam|") || die("Cannot open spam: $!\n");

  # Loop through all the mails
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
        unless ($dbexists && system("$bogofilter <$temp")/256==1) {
          system("$bogofilter -n <$temp");
          $hamadd++;
          $dbexists=1;
          print "Training ham message $hamcount.\n" if ($verbose);
          if ($safe) {
            open (TEMP, ">>$safeham") || die "Cannot write to $safeham: $!";
            print TEMP $mail;
            close (TEMP);
          }
        } else {print "Not training ham message $hamcount.\n" if ($vverbose);}
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
        unless (system("$bogofilter <$temp")/256==0) {
          system("$bogofilter -s <$temp");
          $spamadd++;
          print "Training spam message $spamcount.\n" if ($verbose);
          if ($safe) {
            open (TEMP, ">>$safespam") || die "Cannot write to $safespam: $!";
            print TEMP $mail;
            close (TEMP);
          }
        } else {print "Not training spam message $spamcount.\n" if ($vverbose);}
        unlink ($temp);
      }
    }

  } until (eof(HAM) && eof(SPAM));
  close (HAM);
  close (SPAM);

  print "\nEnd of run #$runs:\n";
  print "Read $hamcount ham mails and $spamcount spam mails.\n";
  print "Added $hamadd ham mail",$hamadd!=1&&"s"," and $spamadd spam mail",$spamadd!=1&&"s"," to the database.\n";
  print `bogoutil -w $dir .MSG_COUNT`;
  $fn=`cat $spam | $bogofilter -vtM | egrep -cv ^[YS]`;
  print "\nFalse negatives: $fn";
  $fp=`cat $ham | $bogofilter -vtM| egrep -cv ^[NH]`;
  print "False positives: $fp\n";
} until ($fn+$fp==0 || !$force);
print "\n$runs run",$runs>1&&"s"," needed to close off.\n" if ($force);
