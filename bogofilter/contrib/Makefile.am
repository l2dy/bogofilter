# $Id$ 

noinst_PROGRAMS=bogogrep
if ENABLE_STATIC
noinst_PROGRAMS+=bogogrep_static
bogogrep_static_SOURCES=bogogrep.c
bogogrep_static_LDFLAGS=$(STATICLDFLAGS)
endif

EXTRA_DIST= \
	README.contrib \
	README.randomtrain \
	bogo.R \
	bfproxy.pl \
	bogofilter-milter.pl \
	bogofilter-qfe.sh \
	bogominitrain.pl \
	dot-qmail-bogofilter-default \
	mailfilter.example \
	mime.get.rfc822.pl \
	parmtest.sh \
	printmaildir.pl \
	procmailrc.example \
	randomtrain.bash \
	scramble.bash \
	spamitarium.pl \
	stripsearch.pl \
	trainbogo.sh
