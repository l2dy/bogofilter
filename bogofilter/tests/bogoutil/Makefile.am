# $Id$

TESTSCRIPTS = driver.sh t.dump.load
TESTS=$(TESTSCRIPTS)

TESTS_ENVIRONMENT = RUN_FROM_MAKE=1 srcdir=$(srcdir) $(SHELL) $(VERBOSE)

EXTRA_DIST=$(TESTSCRIPTS) input-1.txt input-2-data.txt \
	    input-3-data.txt input-4-data.txt output-1.txt \
	    output-2.txt output-3.txt output-4.txt \
	    input-2.count input-3.count input-4.count \
	    run.sh t.dump.load.inp t.dump.load.upd \
	    output-dl-1.txt \
	    output-dl-2.txt \
	    output-dl-3.txt \
	    output-dl-4.txt \
	    output-dl-5.txt \
	    output-dl-6.txt

distclean-local:
	rm -f {in,out}put-[1-9].db
