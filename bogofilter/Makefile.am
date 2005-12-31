# $Id$ 

# search path for m4 macros
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = trio src . doc
SYSCONFDIR = @sysconfdir@

# what to build
sysconf_DATA=	bogofilter.cf.example

# ChangeLog, cvs2cl lives at http://www.red-bean.com/cvs2cl
.PHONY:		$(srcdir)/changelog.txt
$(srcdir)/changelog.txt:
		test -f $@ || : >$@ || { rm -f $@ ; exit 1; }
		( cd $(srcdir) && cvs2cl.pl --fsf --accum --utc --file changelog.txt )

# what to distribute
EXTRA_DIST=	autogen.sh gsl config.rpath \
		bogofilter.cf.example \
		bogofilter.spec bogofilter.spec.in \
		Doxyfile gnugetopt \
		GETTING.STARTED \
		NEWS.0 \
		README.cvs \
		RELEASE.NOTES \
		install-staticdblibs.sh

.PHONY:	check rpm cvs-check

#
# RPM building - there are some cheats here
#
# BF_ZAP_LIBDB is a magic variable we check from configure.ac,
# which zaps LIBDB and allows us to override the library choice
# through LIBS=...
#
# Passing -ldb-4.2 (for the second -tb line) will fail on systems that
# don't have libdb-4.2.* in the default library path that "ld" uses.
# Install your system's db42 and db42-devel packages and retry.
# Using -ldb-4.2 is indispensable as systems with db 4.3 or 4.4 would
# otherwise pick the newer one as their default, but we must match
# the library between shared and static RPM so that users can switch
# forth and back without difficulties such as dumping and reloading
# the database.
#
# CPPFLAGS=-D__NO_CTYPE tells glibc to use the older glibc 2.2.X
# ctype implementation (functions). Else glibc would use a newer
# 2.3.X ctype implementation, and as this is the only module that
# requires 2.3.X in our code, we just use the 2.2.X version instead.
# This works up to and including 2.3.5, the newest glibc version
# available at the time this code was written.
#
DBPFX=/opt/db-4.2-lean
SQPFX=/opt/sqlite-3-lean
DBLIB=$(DBPFX)/lib/libdb.a
SQLIB=$(SQPFX)/lib/libsqlite3.a
DEF_DB42=--define 'with_db42 1'
DEF_SQLITE=--define 'with_sqlite 1'
DEF_STATIC=--define 'bf_nameext -static'
SIGN=--sign
#
rpm:	dist
	@echo
	@echo "Note: if 'make rpm' fails, check the top-level Makefile.am for documentation."
	@echo
	$(RPMBUILD) $(SIGN) -ts $(distdir).tar.gz
	if test ! -f $(DBLIB) || test ! -f $(SQLIB) || objdump -t $(SQLIB) | grep -q __ctype_b_loc ; then \
	  echo >&2 "Please run the install-staticdblibs.sh script first." ; exit 1 ; fi
	BF_ZAP_LIBDB=zap CPPFLAGS="-D__NO_CTYPE -I$(DBPFX)/include" LIBS=$(DBLIB) $(RPMBUILD) $(DEF_STATIC) $(DEF_DB42) $(SIGN) -tb $(distdir).tar.gz
	                 CPPFLAGS="-D__NO_CTYPE -I/usr/include/db42" LIBS=-ldb-4.2 $(RPMBUILD)               $(DEF_DB42) $(SIGN) -tb $(distdir).tar.gz
	BF_ZAP_LIBDB=zap CPPFLAGS="-D__NO_CTYPE -I$(SQPFX)/include" LIBS=$(SQLIB) $(RPMBUILD) $(DEF_STATIC) $(DEF_SQLITE) $(SIGN) -tb $(distdir).tar.gz
	                 CPPFLAGS="-D__NO_CTYPE                   "               $(RPMBUILD)               $(DEF_SQLITE) $(SIGN) -tb $(distdir).tar.gz

cvs-check:
	rm -rf $(PACKAGE)-$(VERSION)-export
	cvs -d "`cat $(srcdir)/CVS/Root`" export -rHEAD -d $(PACKAGE)-$(VERSION)-export $(PACKAGE)
	cd $(PACKAGE)-$(VERSION)-export && autoreconf -i -f -s \
	&& ./configure && make distcheck

# this target expects a .rsyncs file with lines of this format:
# host:directory/
# it will call rsync from its source directory to the destination for
# each of them, running them in parallel
.PHONY: rsync rsynconly
rsync:	distdir rsynconly
rsynconly: .rsyncs
	@( cat $(srcdir)/.rsyncs | sed -e 's}^}rsync -av -e ssh --delete $(PACKAGE)-$(VERSION)/ }; s/\($$\)/ \&/;' ; echo "wait" ) | $(SHELL) -x

dist-hook:
	cd $(distdir) && find doc gnugetopt gsl -name CVS -type d -prune -exec rm -r '{}' ';'
	cd $(distdir) && find doc gnugetopt gsl -name '.#*' -type f -exec rm '{}' ';'
	rm -f $(distdir)/doc/.cvsignore $(distdir)/ChangeLog

# ---------------------------------------------------------------------
# This section deals with the contrib/ directory
#
noinst_PROGRAMS=bogogrep
bogogrep_SOURCES=contrib/bogogrep.c
if ENABLE_STATIC
noinst_PROGRAMS+=bogogrep_static
bogogrep_static_SOURCES=contrib/bogogrep.c
bogogrep_static_LDFLAGS=$(STATICLDFLAGS)
endif

EXTRA_DIST+= \
	contrib/README.contrib \
	contrib/README.randomtrain \
	contrib/bogo.R \
	contrib/bfproxy.pl \
	contrib/bogofilter-milter.pl \
	contrib/bogofilter-qfe.sh \
	contrib/bogominitrain.pl \
	contrib/dot-qmail-bogofilter-default \
	contrib/mailfilter.example \
	contrib/mime.get.rfc822.pl \
	contrib/parmtest.sh \
	contrib/printmaildir.pl \
	contrib/procmailrc.example \
	contrib/randomtrain.sh \
	contrib/scramble.sh \
	contrib/spamitarium.pl \
	contrib/stripsearch.pl \
	contrib/trainbogo.sh
# ---------------------------------------------------------------------
