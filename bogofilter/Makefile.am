# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version
AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS = . tests doc contrib
SYSCONFDIR = @sysconfdir@

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer
bin_SCRIPTS = bogoupgrade
check_PROGRAMS = debugtest configtest wordhash find_home.test
man_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1 bogolexer.1
sysconf_DATA = bogofilter.cf.example

# what to build that from
MYCOMMON= xmalloc.h xmalloc.c xstrdup.h xstrdup.c globals.h debug.h \
	debug.c find_home.h find_home.c find_home_user.c \
	find_home_tildeexpand.c bogoconfig.h

BUILT_SOURCES = version.h directories.h

CLEANFILES=directories.h

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c lexer.l lexer.h \
		     datastore.h datastore_db.h datastore_db.c \
		     config.c directories.h register.c register.h \
		     graham.c graham.h robinson.c robinson.h \
		     rstats.h rstats.c \
                     wordhash.h wordhash.c wordlists.h wordlists.c \
		     $(MYCOMMON) common.h method.h version.h

directories.h: Makefile
	echo >$@ '#define SYSCONFDIR "$(SYSCONFDIR)"' || rm -f $@

bogolexer_SOURCES = bogolexer.c lexer.l lexer.h $(MYCOMMON)

debugtest_SOURCES = debug.c debug.h debug.main.c

bogoutil_SOURCES = bogoutil.c \
		   datastore.h datastore_db.h datastore_db.c wordlists.h wordlists.c \
		   $(MYCOMMON)

configtest_SOURCES = configtest.c config.c directories.h wordlists.c \
		     datastore_db.c $(MYCOMMON)

wordhash_SOURCES = wordhash.c wordhash.h wordhash.main.c $(MYCOMMON)

find_home_test_SOURCES = find_home.test.c $(MYCOMMON)

# what to distribute
EXTRA_DIST = $(man_MANS) \
	     bogofilter.cf.example bogofilter-faq.html \
	     README.hp-ux UPGRADE \
	     bogofilter.xml bogoutil.xml bogoupgrade.xml bogolexer.xml \
	     bogofilter.spec bogofilter.spec.in \
	     bogoupgrade bogo.R \
	     version.sh \
	     README.cvs README.freebsd
#
#
VERSION_FROM=main.c bogofilter.c bogoutil.c lexer.l
version.h:	version.sh $(VERSION_FROM)
	$(srcdir)/version.sh $(srcdir) $(VERSION_FROM) >$@ || rm -f $@
#
.xml.pdf:
	xmlto pdf --extensions $<

.xml.html:
	xmlto html-nochunks $<

.xml.1:
	xmlto man $<

.PHONY: rpm cvs-check

rpm:	    dist $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

cvs-check:
	rm -rf $(PACKAGE)-$(VERSION)-export
	cvs export -rHEAD -d $(PACKAGE)-$(VERSION)-export $(PACKAGE)
	cd $(PACKAGE)-$(VERSION)-export && autoreconf -i -f -s \
	&& ./configure && make distcheck
