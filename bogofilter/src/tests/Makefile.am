# $Id$

SUBDIRS=. bogoutil bogofilter

check_PROGRAMS=dehex spam_header_name dumbhead deqp deb64 escnp
dehex_SOURCES=dehex.c
dumbhead_SOURCES=dumbhead.c
deqp_SOURCES=deqp.c
deb64_SOURCES=deb64.c
escnp_SOURCES=escnp.c
spam_header_name_SOURCES=spam_header_name.c

AM_CPPFLAGS = -I$(srcdir)/..
AM_LDFLAGS = -L..
LDADD = -lbogofilter

TESTSCRIPTS=t.env \
	    t.ignore_spam_header \
	    t.integrity t.integrity2 t.integrity3 \
	    t.lexer \
	    t.lock1 t.lock2 \
	    t.nullstatsprefix \
	    t.score1 t.score2

TESTS=$(TESTSCRIPTS)

TESTS_ENVIRONMENT=RUN_FROM_MAKE=1 AWK=$(AWK) srcdir=$(srcdir) $(SHELL) $(VERBOSE)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.save t.skel inputs outputs

dist-hook:
	cd $(distdir) && for i in inputs outputs ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
