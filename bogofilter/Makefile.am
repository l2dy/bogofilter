# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version
AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS = . tests

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer
bin_SCRIPTS = bogoupgrade
check_PROGRAMS = debugtest configtest wordhash find_home.test
man1_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1

# what to build that from
MYCOMMON= xmalloc.h xmalloc.c xstrdup.h xstrdup.c globals.h debug.h \
    debug.c find_home.h find_home.c bogoconfig.h

BUILT_SOURCES = version.h

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c lexer.l lexer.h \
		     datastore.h datastore_db.h datastore_db.c \
		     config.c \
		     rstats.h rstats.c \
                     wordhash.h wordhash.c wordlists.h wordlists.c \
		     $(MYCOMMON) common.h version.h

bogolexer_SOURCES = bogolexer.c lexer.l lexer.h $(MYCOMMON)

debugtest_SOURCES = debug.c debug.h debug.main.c

bogoutil_SOURCES = bogoutil.c \
		   datastore.h datastore_db.h datastore_db.c wordlists.h wordlists.c \
		   $(MYCOMMON)

configtest_SOURCES = configtest.c config.c wordlists.c datastore_db.c $(MYCOMMON)

wordhash_SOURCES = wordhash.c wordhash.h wordhash.main.c $(MYCOMMON)

find_home_test_SOURCES = find_home.test.c $(MYCOMMON)

# what to distribute
EXTRA_DIST = $(man1_MANS) \
	     bogofilter.cf bogofilter-faq.html \
	     README.hp-ux UPGRADE \
	     bogofilter.xml bogoutil.xml bogoupgrade.xml bogolexer.xml \
	     bogofilter.spec bogofilter.spec.in \
	     bogoupgrade bogo.R \
	     doc/integrating-with-qmail contrib/bogofilter-qfe \
	     version.sh
#
VERSION_FROM=main.c bogofilter.c bogoutil.c lexer.l
$(srcdir)/version.h:	version.sh $(VERSION_FROM)
	$(srcdir)/version.sh $(srcdir) $(VERSION_FROM) >$@ || rm -f $@
#
.xml.pdf:
	xmlto pdf --extensions $<

.xml.html:
	xmlto html-nochunks $<

.xml.1:
	xmlto man $<

.PHONY: rpm
rpm:	    dist $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz
