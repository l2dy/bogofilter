# $Id$

TESTSCRIPTS = t.systest t.grftest
TESTS=$(TESTSCRIPTS)

TESTS_ENVIRONMENT = RUN_FROM_MAKE=1 srcdir=$(srcdir) $(SHELL) $(VERBOSE)

EXTRA_DIST=$(TESTSCRIPTS) inputs outputs

distclean-local:
	rm -f {in,out}put-[1-9].db

dist-hook:
	cd $(distdir) && for i in inputs outputs ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
