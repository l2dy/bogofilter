# $Id$

SUBDIRS=. bogoutil

check_PROGRAMS=dehex spam_header_name
dehex_SOURCES=dehex.c
spam_header_name_SOURCES=spam_header_name.c

t.integrity: dehex

TESTSCRIPTS=t.score1 t.score2 t.integrity t.lock1 t.ignore_spam_header
TESTS=$(TESTSCRIPTS)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.skel
