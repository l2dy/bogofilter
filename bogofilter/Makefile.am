# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version of automake/aclocal
AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS = src . doc contrib tuning
SYSCONFDIR = @sysconfdir@
DISTCHECK_CONFIGURE_FLAGS= @DISTCHECK_CONFIGURE_FLAGS@

# what to build
sysconf_DATA = bogofilter.cf.example

# what to build that from

# what to distribute
EXTRA_DIST = autogen.sh \
	     bogofilter.cf.example \
	     CHANGES-0.10 CHANGES-0.11 CHANGES-0.12 CHANGES-0.13\
	     METHODS UPGRADE Doxyfile \
	     README.cvs RELEASE.NOTES-0.11 RELEASE.NOTES-0.12  RELEASE.NOTES-0.13 \
	     bogofilter.spec bogofilter.spec.in

.PHONY: rpm cvs-check

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
