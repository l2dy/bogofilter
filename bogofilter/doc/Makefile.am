# $Id$ 

# what to build
man_MANS = bogofilter.1 bogoutil.1 bogoupgrade.1 bogolexer.1
html_MANS = bogofilter.html bogoutil.html bogoupgrade.html bogolexer.html

# what to build that from

# what to distribute
EXTRA_DIST = $(man_MANS) \
	     $(html_MANS) \
	     bogofilter-faq.html \
	     bogofilter.xml bogoutil.xml bogoupgrade.xml bogolexer.xml \
	     bogofilter-SA-2002-01 \
	     integrating-with-qmail \
	     README.Robinson \
	     programmer/README.freebsd programmer/README.hp-ux \
	     programmer/README.dcdflib

#	     bogofilter.cf.example 
#	     strlcat.3 strlcpy.3

.xml.pdf:
	xmlto pdf --extensions $<

.xml.html:
	xmlto html-nochunks $<

.xml.1:
	xmlto man $< || { rm -f $@ ; exit 1 ; }
