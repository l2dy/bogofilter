#!/bin/sh -xe
#Test bogoutil and upgrade.pl

#Test 1 -- combined count/data text file
#Test 2 -- separate count, BDB data file.
#Test 3 -- Same as 2 except that BDB file has a .count token.
#Test 4 -- Same as 2 except that BDB file has a  .MSG_COUNT token. Technically already upgraded.
 
for num in `seq 1 4`;do
echo "Running test $num"
(
	inputfile="tests/bogoutil/input-${num}"
	for ext in txt count; do
	  t="${inputfile}.$ext" 
 	  if [ -f $t ]; then
	     inputfile=$t
	   break
	  fi
	done

	if ! [ -e "$inputfile" ]; then
	  break;
	fi

	test -f tests/bogoutil/output-${num}.db && rm -f tests/bogoutil/output-${num}.db

	datafile="tests/bogoutil/input-${num}-data.txt"
	inputdb="tests/bogoutil/input-${num}.db"

	if [ -f "$datafile" ]; then
		rm -f $inputdb
		./bogoutil -l $inputdb <  $datafile
	fi

	outputdb="tests/bogoutil/output-${num}.db"

	perl bogoupgrade.pl -b ./bogoutil -i $inputfile -o $outputdb && \
	./bogoutil -d $outputdb |sort|diff -u - tests/bogoutil/output-${num}.txt
) || echo "failed test $num"
done

