# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version of automake/aclocal
AUTOMAKE_OPTIONS = foreign 1.6
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
endif

AM_CPPFLAGS = -I$(srcdir)/dcdflib/doc
AM_CFLAGS = -DBOGOFILTER

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer
bin_SCRIPTS = bogoupgrade
check_PROGRAMS = debugtest configtest wordhash find_home.test bogowordfreq
man_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1 bogolexer.1
sysconf_DATA = bogofilter.cf.example

noinst_LIBRARIES = libbogofilter.a
LDADD = libbogofilter.a


# what to build that from
libbogofilter_a_SOURCES= \
	xmalloc.h xcalloc.c xmem_error.c xrealloc.c \
	    xmalloc.c xstrdup.h xstrdup.c \
	debug.h debug.c \
	bogoconfig.h config.c \
	find_home.h find_home.c find_home_user.c \
	    find_home_tildeexpand.c \
	collect.h collect.c \
	datastore.h datastore_db.h datastore_db.c \
	lexer.h lexer.l \
	register.h register.c \
	rstats.h rstats.c \
	wordhash.h wordhash.c wordlists.h wordlists.c \
	globals.h system.h version.h \
	dcdflib/src/dcdflib.c dcdflib/src/ipmpar.c

BUILT_SOURCES = version.h directories.h

CLEANFILES= directories.h

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c \
		     $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		     common.h method.h

bogowordfreq_SOURCES = bogowordfreq.c

bogolexer_SOURCES = bogolexer.c

debugtest_SOURCES = debug.main.c

bogoutil_SOURCES = bogoutil.c

configtest_SOURCES = configtest.c

wordhash_SOURCES = wordhash.main.c

find_home_test_SOURCES = find_home.test.c

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
