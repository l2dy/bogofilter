# $Id$

check_PROGRAMS=dehex spam_header_name dumbhead deqp deb64 escnp abortme
dehex_SOURCES=dehex.c
dumbhead_SOURCES=dumbhead.c
deqp_SOURCES=deqp.c
deb64_SOURCES=deb64.c
escnp_SOURCES=escnp.c
spam_header_name_SOURCES=spam_header_name.c

AM_CPPFLAGS = -I$(srcdir)/..
AM_LDFLAGS = -L..
LDADD = -lbogofilter

BUILT_SOURCES = t.config
CLEANFILES= $(BUILT_SOURCES)

t.config:
	echo >$@ 'EXE_EXT="@EXE_EXT@"' || rm -f $@

ENVIRON_TESTS = t.abort t.env t.bogodir

PARSING_TESTS = \
	t.ignore_spam_header \
	t.nullstatsprefix \
	t.integrity t.integrity2 t.integrity3 \
	t.passthrough-hb \
	t.escaped.html \
	t.split t.parsing \
	t.lexer t.lexer.mbx \
	t.folded_rfc2047 \
	t.degen

WORDLIST_TESTS = t.dump.load t.nonascii.replace t.maint t.robx t.regtest t.separate

SCORING_TESTS =  t.score1 t.score2 t.systest t.grftest

BULKMODE_TESTS = t.bulkmode t.MH t.maildir

INTEGRITY_TESTS = t.lock1 t.lock2 t.valgrind

TESTSCRIPTS = ${ENVIRON_TESTS} ${PARSING_TESTS} ${WORDLIST_TESTS} ${SCORING_TESTS} ${BULKMODE_TESTS} ${INTEGRITY_TESTS} 

TESTS=$(TESTSCRIPTS)

TESTS_ENVIRONMENT=RUN_FROM_MAKE=1 AWK=$(AWK) srcdir=$(srcdir) $(SHELL) $(VERBOSE)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.save t.skel inputs outputs \
	printcore t._abort \
	run.sh

dist-hook:
	cd $(distdir) && for i in inputs outputs ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
