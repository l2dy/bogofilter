# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version of automake/aclocal
AUTOMAKE_OPTIONS = foreign 1.6
SYSCONFDIR = @sysconfdir@
DISTCHECK_CONFIGURE_FLAGS= @DISTCHECK_CONFIGURE_FLAGS@

# what to build
man_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1 bogolexer.1

# what to build that from

# what to distribute
extradistdirs = doc
EXTRA_DIST = $(man_MANS) \
	     bogofilter.cf.example bogofilter-faq.html \
	     bogofilter.xml bogoutil.xml bogoupgrade.xml bogolexer.xml \
	     README.cvs README.freebsd README.hp-ux \
	     README.dcdflib README.Robinson \
	     $(extradistdirs) \
	     bogofilter-SA-2002-01 \
	     strlcat.3 strlcpy.3
#
.xml.pdf:
	xmlto pdf --extensions $<

.xml.html:
	xmlto html-nochunks $<

.xml.1:
	xmlto man $< || { rm -f $@ ; exit 1 ; }

#.PHONY: rpm cvs-check

dist-hook:
	cd $(distdir) && for i in $(extradistdirs) ; do find $$i -name CVS -type d -prune -exec rm -r '{}' ';' ; done
	cd $(distdir) && for i in $(extradistdirs) ; do find $$i -name .cvsignore -type f -exec rm -r '{}' ';' ; done
