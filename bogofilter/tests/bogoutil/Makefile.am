# $Id$

TESTSCRIPTS=run.sh
TESTS=$(TESTSCRIPTS)

EXTRA_DIST=$(TESTSCRIPTS) input-1.txt input-2-data.txt \
	    input-3-data.txt input-4-data.txt output-1.txt \
	    output-2.txt output-3.txt output-4.txt \
	    input-2.count input-3.count input-4.count

distclean-local:
	rm -f {in,out}put-[1-9].db
