#! /bin/sh
#Test bogoutil and upgrade.pl

#Test 1 -- combined count/data text file
#Test 2 -- separate count, BDB data file.
#Test 3 -- Same as 2 except that BDB file has a .count token.
#Test 4 -- Same as 2 except that BDB file has a  .MSG_COUNT token.
#          Technically already upgraded.

set -ex

for num in `seq 1 4`;do
echo "Running test $num"
(
	inputfile="input-${num}"
	for ext in txt count; do
	  t="${inputfile}.$ext" 
	  if [ -f $srcdir/$t ]; then
	     inputfile=$t
	   break
	  fi
	done

	if [ ! -f "$srcdir/$inputfile" ]; then
	  exit 2;
	fi

	killinput=0
	if [ "$srcdir" != "." ] ; then
	    cp -f $srcdir/$inputfile x$inputfile
	    inputfile=x$inputfile
	    killinput=1
	fi
	
	datafile="${srcdir}/input-${num}-data.txt"
	inputdb="input-${num}.db"

	if [ -f "$datafile" ]; then
		rm -f $inputdb
		../../bogoutil -l $inputdb <  $datafile
	fi

	outputdb="output-${num}.db"
	rm -f $outputdb

	perl ${srcdir}/../../bogoupgrade.pl -b ../../bogoutil -i $inputfile -o $outputdb && \
	../../bogoutil -d $outputdb |LC_COLLATE=C sort\
	|diff -u - ${srcdir}/output-${num}.txt
	rm -f $outputdb
	rm -f $inputdb
	[ $killinput -eq 0 ] || rm -f $inputfile
) || { echo "failed test $num" ; exit 1 ; }
done

