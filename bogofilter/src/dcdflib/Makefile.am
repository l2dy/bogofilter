SUBDIRS=src
EXTRA_DIST=HOWTOGET doc

dist-hook:
	cd $(distdir) && find doc -name CVS -type d -prune -exec rm -r '{}' ';'
	rm -f $(distdir)/doc/.cvsignore
