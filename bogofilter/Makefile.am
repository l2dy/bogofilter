# $Id$ 

# search path for m4 macros
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = trio src . doc contrib
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
		README.cvs \
		RELEASE.NOTES

.PHONY:	check

rpm:	dist
	case `rpm --version` in \
	  "Version 3*")	rpm --define 'bogostatic 1' --sign -ta $(distdir).tar.gz ;; \
	  *)		rpmbuild --define 'bogostatic 1' --sign -ta $(distdir).tar.gz ;; \
	esac

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
