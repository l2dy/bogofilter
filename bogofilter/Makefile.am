# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version
AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS = . tests

# what to build
bin_PROGRAMS = bogofilter bogoutil
bin_SCRIPTS = bogoupgrade.pl
check_PROGRAMS = lexertest
man1_MANS = bogofilter.1 bogoutil.1

# what to build that from
MYCOMMON= xmalloc.h xmalloc.c xstrdup.h xstrdup.c

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c lexer.l lexer.h \
		     datastore.h datastore_db.h datastore_db.c \
                     wordhash.h wordhash.c wordlists.h wordlists.c \
		     $(MYCOMMON) common.h version.h

lexertest_SOURCES = lexertest.c lexer.l lexer.h

bogoutil_SOURCES = bogoutil.c \
		   datastore.h datastore_db.h datastore_db.c \
		   $(MYCOMMON)

# what to distribute
EXTRA_DIST = $(man1_MANS) \
	     bogofilter.xml bogoutil.xml \
	     bogofilter.spec bogofilter.spec.in \
	     UPGRADE bogoupgrade.pl \
	     doc/integrating-with-qmail contrib/bogofilter-qfe \
	     version.sh
#
version.h:	main.c bogofilter.c
	version.sh $^ > $@
#
.xml.pdf:
	xmlto pdf --extensions $<

.xml.html:
	xmlto html $<
	mv -f index.html $@

.xml.1:
	xmlto man $<

.PHONY: rpm
rpm:	    dist $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz
