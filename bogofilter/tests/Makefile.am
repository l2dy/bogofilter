# $Id$

SUBDIRS=. bogoutil t.systest.d

check_PROGRAMS=dehex spam_header_name dumbhead deqp deb64
dehex_SOURCES=dehex.c
dumbhead_SOURCES=dumbhead.c
deqp_SOURCES=deqp.c
deb64_SOURCES=deb64.c
spam_header_name_SOURCES=spam_header_name.c

AM_CPPFLAGS = -I$(srcdir)/..
AM_LDFLAGS = -L..
LDADD = -lbogofilter

t.integrity: dehex dumbhead

TESTSCRIPTS=t.score1 t.score2 t.integrity t.integrity2 \
	    t.lock1 t.lock2 t.ignore_spam_header t.env \
	    t.nullstatsprefix t.lexer
TESTS=$(TESTSCRIPTS)

TESTS_ENVIRONMENT=RUN_FROM_MAKE=1 srcdir=$(srcdir) $(SHELL) $(VERBOSE)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.skel inputs outputs

dist-hook:
	cd $(distdir) && for i in inputs outputs ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
