# $Id$ 

SUBDIRS = trio src . doc contrib
SYSCONFDIR = @sysconfdir@

# what to build
sysconf_DATA=	bogofilter.cf.example

# ChangeLog, cvs2cl lives at http://www.red-bean.com/cvs2cl
.PHONY:		changelog.txt
changelog.txt:
		test -f $@ || : >$@ || { rm -f $@ ; exit 1; }
		cvs2cl.pl --fsf --accum --utc --file $@

# what to distribute
EXTRA_DIST=	autogen.sh gsl config.rpath \
		bogofilter.cf.example \
		bogofilter.spec bogofilter.spec.in \
		METHODS Doxyfile \
		CHANGES-0.10 README.cvs \
		CHANGES-0.11 RELEASE.NOTES-0.11 \
		CHANGES-0.12 RELEASE.NOTES-0.12 \
		CHANGES-0.13 RELEASE.NOTES-0.13 \
		CHANGES-0.14 RELEASE.NOTES-0.14 \
		CHANGES-0.15 RELEASE.NOTES-0.15 \
		CHANGES-0.16 RELEASE.NOTES-0.16

.PHONY:	rpm cvs-check

rpm:	dist
	rpm -ta $(distdir).tar.gz

cvs-check:
	rm -rf $(PACKAGE)-$(VERSION)-export
	cvs export -rHEAD -d $(PACKAGE)-$(VERSION)-export $(PACKAGE)
	cd $(PACKAGE)-$(VERSION)-export && autoreconf -i -f -s \
	&& ./configure && make distcheck

# this target expects a .rsyncs file with lines of this format:
# host:directory/
# it will call rsync from its source directory to the destination for
# each of them, running them in parallel
rsync:	distdir .rsyncs
	@( cat .rsyncs | sed -e 's}^}rsync -av --delete $(PACKAGE)-$(VERSION)/ }; s/\($$\)/ \&/;' ; echo "wait" ) | $(SHELL) -x

dist-hook:
	cd $(distdir) && find doc gsl -name CVS -type d -prune -exec rm -r '{}' ';'
	rm -f $(distdir)/doc/.cvsignore
