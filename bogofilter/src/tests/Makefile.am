# $Id$

check_PROGRAMS=dehex spam_header_name dumbhead deqp deb64 escnp abortme \
	       u_fpe wantcore

AM_CPPFLAGS = -I$(srcdir)/..
AM_LDFLAGS = -L..
LDADD = -lbogofilter

BUILT_SOURCES = t.config
CLEANFILES= $(BUILT_SOURCES)

t.config: Makefile
	echo >$@ 'EXE_EXT="@EXE_EXT@"' || rm -f $@

ENVIRON_TESTS = t.abort t.env t.bogodir t.u_fpe

PARSING_TESTS = \
	t.ignore_spam_header \
	t.nullstatsprefix \
	t.integrity t.integrity2 t.integrity3 \
	t.passthrough-hb \
	t.escaped.html t.escaped.url \
	t.split t.parsing \
	t.lexer t.lexer.mbx \
	t.folded_rfc2047 \
	t.msgaddr
# t.message_id - commented out, outputs/message_id.ref not in CVS

WORDLIST_TESTS = t.dump.load t.nonascii.replace t.maint t.robx t.regtest t.upgrade.subnet.prefix t.multiple.wordlists

SCORING_TESTS =  t.score1 t.score2 t.systest t.grftest t.wordhist

BULKMODE_TESTS = t.bulkmode t.MH t.maildir

INTEGRITY_TESTS = t.lock1 t.lock3 t.valgrind
# INTEGRITY_TESTS += t.lock2

TESTSCRIPTS = ${ENVIRON_TESTS} ${PARSING_TESTS} ${WORDLIST_TESTS} ${SCORING_TESTS} ${BULKMODE_TESTS} ${INTEGRITY_TESTS}

TESTS=$(TESTSCRIPTS)

TESTS_ENVIRONMENT=RUN_FROM_MAKE=1 AWK=$(AWK) srcdir=$(srcdir) $(SHELL) $(VERBOSE)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.save t.skel \
	printcore t._abort unsort.pl \
	run.sh \
	inputs/dump.load.inp \
	inputs/dump.load.upd \
	inputs/good.mbx \
	inputs/headerbody.txt \
	inputs/lexer.inp \
	inputs/lexer.mbx \
	inputs/lexer.out \
	inputs/msg.1.txt \
	inputs/msg.2.txt \
	inputs/msg.3.txt \
	inputs/msg.4.txt \
	inputs/msg.5.txt \
	inputs/msg.6.txt \
	inputs/msg.7.txt \
	inputs/msg.8.txt \
	inputs/msg.flex \
	inputs/msg.parsing.txt \
	inputs/msg.regtest.n \
	inputs/msg.regtest.s \
	inputs/msg.split.bg.0117.text \
	inputs/msg.split.dr.0118.base64 \
	inputs/msg.split.gs.0119.text \
	inputs/spam.mbx \
	outputs/MH.out \
	outputs/bogolex.out \
	outputs/bulkmode.out \
	outputs/combined.out \
	outputs/degen.out \
	outputs/dump.load-1.out \
	outputs/dump.load-2.out \
	outputs/dump.load-3.out \
	outputs/dump.load-4.out \
	outputs/dump.load-5.out \
	outputs/dump.load-6.out \
	outputs/grftest.out \
	outputs/lexer.out \
	outputs/maildir.out \
	outputs/msg.1.f.v \
	outputs/msg.1.g.v \
	outputs/msg.1.r.v \
	outputs/msg.2a.f.vv \
	outputs/msg.2a.f.vvv \
	outputs/msg.2a.g.vv \
	outputs/msg.2a.g.vvv \
	outputs/msg.2a.r.vv \
	outputs/msg.2a.r.vvv \
	outputs/msg.2b.f.vv \
	outputs/msg.2b.f.vvv \
	outputs/msg.2b.g.vv \
	outputs/msg.2b.g.vvv \
	outputs/msg.2b.r.vv \
	outputs/msg.2b.r.vvv \
	outputs/msg.3.f.vvv \
	outputs/msg.3.g.vvv \
	outputs/msg.3.r.vvv \
	outputs/msgaddr.ref \
	outputs/multiple.wordlists.ref   \
	outputs/parsing.out \
	outputs/separate.out \
	outputs/split.out
