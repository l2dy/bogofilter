#!/usr/bin/perl

=pod

Name:
upgrade.pl -- upgrade a bogofilter database to current version.

Author:
Gyepi Sam <gyepi@praxis-sw.com>


=cut

my $VERSION = '0.1';

my ($in, $out, $help);

my $bogoutil = 'bogoutil';

for (my $i = 0; $i < @ARGV; $i++){

  my $arg = $ARGV[$i];

  if ($arg eq '-i'){
    $in = $ARGV[++$i];
  }
  elsif ($arg eq '-o'){
    $out = $ARGV[++$i];
  }
  elsif ($arg eq '-b'){
    $bogoutil = $ARGV[++$i];
  }
  elsif ($arg eq '-h'){
    help();
    exit(1);
  }
  else {
    usage();
    exit(1);
  }
}

my $msg_count_token = '.MSG_COUNT';

open(F, $in) or die "Cannot open input file [$in]. $!.\n";
my $sig = <F>;
chomp($sig);

if ($sig =~ m/^\# bogofilter wordlist \(format version A\):\s(\d+)$/){ 
  
  my $msg_count = $1;
  my $cmd = qq[$bogoutil -l $out];
  open(OUT, "|$cmd") or die "Cannot run command [$cmd]. $!\n";
  while(<F>){
    print OUT $_;
  }
  print OUT "$msg_count_token $msg_count\n";
  close(OUT);
  close(F);
}
elsif ($sig =~ m/^\# bogofilter email-count \(format version B\):\s(\d+)/){
  my $msg_count = $1;
  my $in_db = $in;
  $in_db =~ s/count$/db/;

  unless (-f $in_db){
    warn("Cannot find database file [$in_db] corresponding to input file [$in]\n");
    exit;
  }
  
  my $cmd = qq[$bogoutil -l $out];
  open(OUT, "|$cmd") or die "Cannot run command [$cmd]. $!\n";

  close(F);
  $cmd = qq[$bogoutil -d $in_db];
  open(F, "$cmd|") or die "Cannot run command [$cmd]. $!\n";

  while(<F>){
    if (m/^\.count\s+(\d+)$/){
      warn("Found a message count of [$1] in db. Throwing away text file count of [$msg_count]\n");
      $msg_count = $1;
      next;
    }
    elsif (/^$msg_count_token\s(\d+)$/){
      warn("This database appears to have been upgraded already. But there's no harm in doing it again.\n");
      $msg_count = $1;
      next;
    }
    print OUT $_;
  }
  print OUT "$msg_count_token $msg_count\n";

  close(F);
  close(OUT);
}
else {
  print STDERR "Cannot recognize signature [$sig].\n";
  exit(2);
}

exit(0);

sub usage {
  print STDERR "usage: $0 [ -i <input text file> -o <output db file> [ -b <path to bogoutil>] ] [ -h ]\n";
}

sub help {
  print <<EOF;
  $0 -- upgrades bogofilter database to current version.
  Options:
    -i	<input file>.
      
        Text file containing message counts, and possibly data.
        If there is no data in the text file, there should be a  Berkeley DB file
        in the same directory as the text file which contains the data. 
            
   -o	<output file>
              
        Output Berkeley DB file.

   -b   <path to bogoutil program>

        Defaults to 'bogoutil', in the hopes that your shell will find it.
                
   -h	help
                  
	You are reading it.
EOF
	exit(0);
}
