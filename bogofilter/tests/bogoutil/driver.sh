#! /bin/sh
#Test bogoutil and upgrade.pl

#Test 1 -- combined count/data text file
#Test 2 -- separate count, BDB data file.
#Test 3 -- Same as 2 except that BDB file has a .count token.
#Test 4 -- Same as 2 except that BDB file has a  .MSG_COUNT token.
#          Technically already upgraded.

set -e

# Note FreeBSD has no "seq" command.
for num in 1 2 3 4 ;do
echo "Running test $num"
  env num=${num} ${SHELL:=/bin/sh} ${srcdir}/run.sh
done

