#!/usr/bin/perl

my $instances = shift || 30;

my $logfile = 'lock.log';

unlink $logfile;

$SIG{CHLD}='IGNORE';

while($instances--){ 
	run(int(rand(10)) % 3);
}

sub run {
	my $i = shift;

	my @r = (
		{flag => '-s', dir => 'spam'},
		{flag => '-h', dir => 'nonspam'},
		{flag => '',   dir => 'test'}
		);

	my $d = $r[$i]->{dir};
	print "$d\n";

	opendir(D, $d);
	my @f = grep { $_ !~ /^\./ } readdir(D);
	closedir(D);
	for my $file (@f){
	  fork() || exec("../../bogofilter -v -v -d . $r[$i]->{flag} < $d/$file >> $logfile 2>&1");
	}
}
