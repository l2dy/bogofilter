# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version of automake/aclocal
AUTOMAKE_OPTIONS = foreign 1.6
SUBDIRS =  dcdflib . tests doc contrib
SYSCONFDIR = @sysconfdir@
DISTCHECK_CONFIGURE_FLAGS= @DISTCHECK_CONFIGURE_FLAGS@

if ENABLE_GRAHAM_METHOD
GRAHAM_SRC = graham.c graham.h
endif

if ENABLE_ROBINSON_METHOD
ROBINSON_SRC = robinson.c robinson.h
endif

if ENABLE_ROBINSON_FISHER
FISHER_SRC = fisher.c fisher.h
endif

AM_CPPFLAGS = -I$(srcdir)/dcdflib/doc
AM_CFLAGS = -DBOGOFILTER

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer 
bin_SCRIPTS = bogoupgrade
check_PROGRAMS = debugtest configtest wordhash find_home.test bogowordfreq
sysconf_DATA = bogofilter.cf.example

noinst_LIBRARIES = libbogofilter.a
LDADD = libbogofilter.a @LIBOBJS@

# what to build that from
libbogofilter_a_SOURCES= \
	common.h system.h \
	globals.h globals.c \
	directories.c version.c \
	base64.h base64.c \
	bogoconfig.h config.c \
	bool.h bool.c \
	charset.h charset.c \
	collect.h collect.c \
	datastore.h datastore_db.c \
	debug.h debug.c \
	error.h error.c \
	fgetsl.h fgetsl.c \
	find_home.h find_home.c find_home_user.c find_home_tildeexpand.c \
	format.h format.c \
	html.h html.c \
	lexer.h lexer.c lexer_head.l lexer_text_plain.l lexer_text_html.l \
	maint.h maint.c \
	mime.h mime.c \
	paths.h paths.c \
	qp.h qp.c \
	register.h register.c \
	rstats.h rstats.c \
	swap.h swap_32bit.c\
	textblock.h textblock.c \
	token.h token.c \
	uudecode.h uudecode.c \
	wordhash.h wordhash.c wordlists.h wordlists.c \
	xmalloc.h xcalloc.c xmalloc.c xmem_error.c xrealloc.c xstrdup.h xstrdup.c \
	xstrlcat.h xstrlcat.c \
	xstrlcpy.h xstrlcpy.c \
	dcdflib/src/dcdflib.c dcdflib/src/ipmpar.c

CLEANFILES=version.c directories.c

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c \
		     method.c $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		     common.h method.h

debugtest_SOURCES = debug.main.c

wordhash_SOURCES = wordhash.main.c

find_home_test_SOURCES = find_home.test.c

# what to distribute
EXTRA_DIST = bogofilter.cf.example \
	     METHODS UPGRADE \
	     README.cvs \
	     bogofilter.spec bogofilter.spec.in \
	     bogoupgrade \
	     version.sh \
	     strlcat.3 strlcpy.3
#
VERSION_FROM=main.c bogofilter.c bogoutil.c lexer.c lexer_head.l lexer_text_plain.l lexer_text_html.l
version.c:	version.sh $(VERSION_FROM)
	$(srcdir)/version.sh $(srcdir) $(VERSION_FROM) >"$@" \
	    || rm -f "$@"
#
directories.c: config.status
	echo "const char *const system_config_file = \"$(SYSCONFDIR)/bogofilter.cf\";" >"$@" \
	    || rm -f "$@"
#
lexer_head.c:	lexer_head.l
	flex -Plexer_ -o$@ $<

lexer_text_plain.c:	lexer_text_plain.l
	flex -Ptext_plain_ -o$@ $<

lexer_text_html.c:	lexer_text_html.l
	flex -Ptext_html_ -o$@ $<
#
.PHONY: rpm cvs-check

rpm:	    dist $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

cvs-check:
	rm -rf $(PACKAGE)-$(VERSION)-export
	cvs export -rHEAD -d $(PACKAGE)-$(VERSION)-export $(PACKAGE)
	cd $(PACKAGE)-$(VERSION)-export && autoreconf -i -f -s \
	&& ./configure && make distcheck
