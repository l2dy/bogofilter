# $Id$

SUBDIRS=. bogoutil

check_PROGRAMS=dehex spam_header_name dumbhead
dehex_SOURCES=dehex.c
dumbhead_SOURCES=dumbhead.c
spam_header_name_SOURCES=spam_header_name.c

t.integrity: dehex dumbhead

TESTSCRIPTS=t.score1 t.score2 t.integrity t.lock1 t.ignore_spam_header
TESTS=$(TESTSCRIPTS)

EXTRA_DIST=$(TESTSCRIPTS) t.frame t.skel
