# $Id$

SUBDIRS=. bogoutil

check_PROGRAMS=dehex
dehex_SOURCES=dehex.c

t.integrity: dehex

TESTSCRIPTS=t.score1 t.score2 t.integrity t.lock1
TESTS=$(TESTSCRIPTS)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.skel
