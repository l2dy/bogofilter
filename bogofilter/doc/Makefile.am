# $Id$ 

# Tell BSD make(1) to not remake the .1in and .htmlin files.
SUFFIXES= .1 .1in .htmlin .html

# what to build
man_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1 bogolexer.1 bogotune.1

html_MANS = bogofilter.html bogoutil.html bogoupgrade.html bogolexer.html bogotune.html

all: $(man_MANS) $(html_MANS)

CLEANFILES = $(man_MANS) $(html_MANS)

# what to distribute
EXTRA_DIST = \
	     bogofilter.xml bogofilter.1in bogofilter.htmlin \
	     bogolexer.xml bogolexer.1in bogolexer.htmlin \
	     bogotune.xml bogotune.1in bogotune.htmlin \
	     bogoupgrade.xml bogoupgrade.1in bogoupgrade.htmlin \
	     bogoutil.xml bogoutil.1in bogoutil.htmlin \
	     bogofilter-faq.html \
	     bogofilter-tuning.HOWTO \
	     bogofilter-SA-2002-01 \
	     integrating-with-postfix \
	     integrating-with-qmail \
	     programmer/README.freebsd programmer/README.hp-ux \
	     programmer/README.RISC-OS \
	     programmer/RISC-OS/src/Makefile \
	     programmer/RISC-OS/gsl/Makefile \
	     README.tdb

#	     bogofilter.cf.example 
#	     strlcat.3 strlcpy.3

.1in.1:
	sed -e "s|[@]sysconfdir@|$(sysconfdir)|g" \
		-e "s|[@]bindir@|$(bindir)|g" <$< >$@ \
	|| { rm -f $@ ; exit 1 ; }

# same as .1in.1: above, but POSIX doesn't allow two on one line.
.htmlin.html:
	sed -e "s|[@]sysconfdir@|$(sysconfdir)|g" \
		-e "s|[@]bindir@|$(bindir)|g" <$< >$@ \
	|| { rm -f $@ ; exit 1 ; }

if ENABLE_XMLTO
# Use Perl to fix up br0ken 0xa0 characters that the XML DocBook style
# sheets emit instead of "\ " for roff or "&nbsp;" for HTML,
# respectively.
.xml.htmlin:
	if test -f $(srcdir)/$@ && test $(srcdir)/$@ -nt $< ; then \
	    cp $(srcdir)/$@ . ; else \
	if xmlto html-nochunks $< ; then f=$@ ; mv $${f%%in} $@ ; else rm -f $@ ; exit 1 ; fi ; \
	perl -pl -e "s|\xA0|&nbsp;|g" -i $@ || rm -f $@ ; \
	fi

.xml.1in:
	if test -f $(srcdir)/$@ && test $(srcdir)/$@ -nt $< ; then \
	    cp $(srcdir)/$@ . ; else \
	if xmlto man $< ; then f=$@ ; mv $${f%in} $@ ; else rm -f $@ ; exit 1 ; fi ; \
	perl -pl -e "s|\xA0|\\ |g" -i $@ || rm -f $@ ; \
	fi
else
.xml.1in:
	@echo 1>&2
	@echo 1>&2 "You need xmlto to build bogofilter documentation."
	@echo 1>&2
	@exit 1

.xml.htmlin:
	@echo 1>&2
	@echo 1>&2 "You need xmlto to build bogofilter documentation."
	@echo 1>&2
	@exit 1
endif
