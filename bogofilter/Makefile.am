# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version
AUTOMAKE_OPTIONS = foreign 1.6

# what to build
bin_PROGRAMS = bogofilter bogoutil
bin_SCRIPTS = bogoupgrade.pl
noinst_PROGRAMS = lexertest
man1_MANS = bogofilter.1

# what to build that from
MYLIB= xmalloc.h xmalloc.c xstrdup.h xstrdup.c

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c lexer.l lexer.h \
		     datastore.h datastore_db.h datastore_db.c \
                     wordhash.h wordhash.c wordlists.h wordlists.c \
		     $(MYLIB) common.h

lexertest_SOURCES = lexer.l
lexertest_CFLAGS = -DMAIN

bogoutil_SOURCES = bogoutil.c \
		   datastore.h datastore_db.h datastore_db.c \
		   $(MYLIB)

# what to distribute
EXTRA_DIST = $(man1_MANS) \
	     bogofilter.xml \
	     bogofilter.spec bogofilter.spec.in \
	     UPGRADE bogoupgrade.pl
	     
#
.xml.1:
	xmlto man $<

.PHONY: rpm
rpm:	    dist $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz
