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

.PHONY:	check

DBPFX=/opt/db-4.2-lean
SQPFX=/opt/sqlite-3-lean
DBLIB=$(DBPFX)/lib/libdb.a
SQLIB=$(SQPFX)/lib/libsqlite3.a
DEF_SQLITE=--define 'with_sqlite 1'
DEF_STATIC=--define 'bogostatic 1'

# BF_ZAP_LIBDB is a magic variable we check from configure.ac
rpm:	dist
	if test ! -f $(DBLIB) || test ! -f $(SQLIB) || objdump -t $(SQLIB) | grep -q __ctype_b_loc ; then \
	  echo >&2 "Please run the install-staticdblibs.sh script first." ; exit 1 ; fi
	BF_ZAP_LIBDB=zap CPPFLAGS="-D__NO_CTYPE -I$(DBPFX)/include" LIBS=$(DBLIB) $(RPMBUILD) --sign -ta $(distdir).tar.gz
	BF_ZAP_LIBDB=zap CPPFLAGS="-D__NO_CTYPE -I$(SQPFX)/include" LIBS=$(SQLIB) $(RPMBUILD) $(DEF_SQLITE) --sign -tb $(distdir).tar.gz

rpm-static:	dist
	if test ! -f $(DBLIB) || test ! -f $(SQLIB) || objdump -t $(SQLIB) | grep -q __ctype_b_loc ; then \
	  echo >&2 "Please run the install-staticdblibs.sh script first." ; exit 1 ; fi
	BF_ZAP_LIBDB=zap CPPFLAGS="-D__NO_CTYPE -I$(DBPFX)/include" LIBS=$(DBLIB) $(RPMBUILD) $(DEF_STATIC) --sign -ta $(distdir).tar.gz
	BF_ZAP_LIBDB=zap CPPFLAGS="-D__NO_CTYPE -I$(SQPFX)/include" LIBS=$(SQLIB) $(RPMBUILD) $(DEF_STATIC) $(DEF_SQLITE) --sign -ta $(distdir).tar.gz

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
