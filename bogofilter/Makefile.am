# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version of automake/aclocal
AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS =  src . doc contrib
SYSCONFDIR = @sysconfdir@
DISTCHECK_CONFIGURE_FLAGS= @DISTCHECK_CONFIGURE_FLAGS@

#AM_CPPFLAGS = -I$(srcdir)/dcdflib/doc
#AM_CFLAGS = -DBOGOFILTER

# what to build
sysconf_DATA = bogofilter.cf.example

#noinst_LIBRARIES = libbogofilter.a
#LDADD = libbogofilter.a @LIBOBJS@

# what to build that from

# what to distribute
EXTRA_DIST = autogen.sh \
	     bogofilter.cf.example \
	     CHANGES-0.10 CHANGES-0.11 \
	     METHODS UPGRADE \
	     README.cvs \
	     bogofilter.spec bogofilter.spec.in
#
.PHONY: rpm cvs-check

rpm:	    dist
	rpm -ta $(distdir).tar.gz

cvs-check:
	rm -rf $(PACKAGE)-$(VERSION)-export
	cvs export -rHEAD -d $(PACKAGE)-$(VERSION)-export $(PACKAGE)
	cd $(PACKAGE)-$(VERSION)-export && autoreconf -i -f -s \
	&& ./configure && make distcheck
