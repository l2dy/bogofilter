# $Id$ 

# foreign: don't insist on ChangeLog
# 1.5: minimum version
AUTOMAKE_OPTIONS = foreign 1.5
SUBDIRS =  dcdflib . tests
SYSCONFDIR = @sysconfdir@

if ENABLE_GRAHAM_METHOD
GRAHAM_SRC = graham.c graham.h
endif

if ENABLE_ROBINSON_METHOD
ROBINSON_SRC = robinson.c robinson.h
endif

if ENABLE_ROBINSON_FISHER
FISHER_SRC = fisher.c fisher.h
AM_CPPFLAGS = -I$(srcdir)/dcdflib/doc
LDADD = dcdflib/src/libdcdf.a
else
LDADD =
endif

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer
bin_SCRIPTS = bogoupgrade
check_PROGRAMS = debugtest configtest wordhash find_home.test
man_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1 bogolexer.1
sysconf_DATA = bogofilter.cf.example

noinst_LIBRARIES = libbogofilter.a
LDADD += libbogofilter.a

# what to build that from
libbogofilter_a_SOURCES=xmalloc.h xcalloc.c xmem_error.c xrealloc.c \
	xmalloc.c xstrdup.h xstrdup.c globals.h debug.h \
	debug.c find_home.h find_home.c find_home_user.c \
	find_home_tildeexpand.c

MYCOMMON=  bogoconfig.h system.h version.h

BUILT_SOURCES = version.h directories.h

CLEANFILES= directories.h

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c lexer.l lexer.h \
		     datastore.h datastore_db.h datastore_db.c \
		     config.c register.c register.h \
		     $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		     rstats.h rstats.c \
                     wordhash.h wordhash.c wordlists.h wordlists.c \
		     $(MYCOMMON) common.h method.h

bogolexer_SOURCES = bogolexer.c lexer.l lexer.h $(MYCOMMON)

debugtest_SOURCES = debug.c debug.h debug.main.c

bogoutil_SOURCES = bogoutil.c \
		   datastore.h datastore_db.h datastore_db.c wordlists.h wordlists.c \
		   $(MYCOMMON)

configtest_SOURCES = configtest.c config.c wordlists.c \
		     datastore_db.c $(MYCOMMON)

wordhash_SOURCES = wordhash.c wordhash.h wordhash.main.c $(MYCOMMON)

find_home_test_SOURCES = find_home.test.c $(MYCOMMON)

# what to distribute
extradistdirs = doc contrib
EXTRA_DIST = $(man_MANS) \
	     bogofilter.cf.example bogofilter-faq.html \
	     METHODS UPGRADE \
	     bogofilter.xml bogoutil.xml bogoupgrade.xml bogolexer.xml \
	     bogofilter.spec bogofilter.spec.in \
	     bogoupgrade bogo.R \
	     version.sh \
	     README.cvs README.freebsd README.hp-ux \
	     README.dcdflib README.Robinson \
	     $(extradistdirs)
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

directories.h: Makefile
	echo "#define SYSCONFDIR \"$(SYSCONFDIR)\"" >$@ || rm -f $@

cvs-check:
	rm -rf $(PACKAGE)-$(VERSION)-export
	cvs export -rHEAD -d $(PACKAGE)-$(VERSION)-export $(PACKAGE)
	cd $(PACKAGE)-$(VERSION)-export && autoreconf -i -f -s \
	&& ./configure && make distcheck

dist-hook:
	cd $(distdir) && for i in $(extradistdirs) ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
	cd $(distdir) && for i in $(extradistdirs) ; do find $$i -name .cvsignore -type f -exec rm -r '{}' ';' ; done
