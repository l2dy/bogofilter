# $Id$

SUBDIRS=. bogoutil

SYSTEM_TESTS=t.systest t.grftest

check_PROGRAMS=dehex spam_header_name dumbhead
dehex_SOURCES=dehex.c
dumbhead_SOURCES=dumbhead.c
spam_header_name_SOURCES=spam_header_name.c

t.integrity: dehex dumbhead

TESTSCRIPTS=t.score1 t.score2 t.integrity t.lock1 t.ignore_spam_header \
	    t.nullstatsprefix t.lexer $(SYSTEM_TESTS)
TESTS=$(TESTSCRIPTS)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.skel t.systest.d inputs outputs

dist-hook:
	cd $(distdir) && for i in t.systest.d inputs outputs ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
