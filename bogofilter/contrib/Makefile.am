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
	bfproxy \
	bogofilter-milter.pl \
	bogofilter-qfe \
	bogominitrain.pl \
	dot-qmail-bogofilter-default \
	mailfilter.example \
	mime.get.rfc822 \
	parmtest.sh \
	printmaildir.pl \
	procmailrc.example \
	randomtrain \
	scramble \
	spamitarium \
	stripsearch \
	trainbogo.sh
