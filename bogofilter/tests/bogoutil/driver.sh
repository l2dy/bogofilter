#! /bin/sh
#Test bogoutil and upgrade.pl

#Test 1 -- combined count/data text file
#Test 2 -- separate count, BDB data file.
#Test 3 -- Same as 2 except that BDB file has a .count token.
#Test 4 -- Same as 2 except that BDB file has a  .MSG_COUNT token.
#          Technically already upgraded.

set -e

for num in `seq 1 4`;do
echo "Running test $num"
  env num=${num} ${srcdir}/run.sh
done

