# $Id$ 

# foreign: don't insist on ChangeLog
# 1.6: minimum version of automake/aclocal
SUBDIRS =  dcdflib . tests
SYSCONFDIR = @sysconfdir@
DISTCHECK_CONFIGURE_FLAGS= @DISTCHECK_CONFIGURE_FLAGS@

if ENABLE_GRAHAM_METHOD
GRAHAM_SRC = graham.c graham.h
endif

if ENABLE_ROBINSON_FISHER
FISHER_SRC = fisher.c fisher.h
ROBINSON_SRC = robinson.c robinson.h
else
if ENABLE_ROBINSON_METHOD
ROBINSON_SRC = robinson.c robinson.h
endif
endif

if ENABLE_MEMDEBUG
MEMDEBUG_SRC = memdebug.c memdebug.h
endif

AM_CPPFLAGS = -I$(srcdir)/dcdflib/doc -I$(top_srcdir)/trio
AM_CFLAGS = -DBOGOFILTER

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer
bin_SCRIPTS = bogoupgrade
if ENABLE_STATIC
bin_PROGRAMS += bogofilter_static bogoutil_static bogolexer_static
endif

check_PROGRAMS = debugtest configtest wordhash find_home.test \
		 bogowordfreq fgetsl.test

noinst_LIBRARIES = libbogofilter.a
TESTS=

if NEEDTRIO
noinst_LIBRARIES += libtrio.a
libtrio_a_SOURCES= ../trio/triostr.c ../trio/trio.c ../trio/trionan.c \
		   ../trio/trio.h ../trio/triop.h ../trio/triodef.h \
		   ../trio/trionan.h ../trio/triostr.h
check_PROGRAMS += regression
TESTS+=./regression
regression_SOURCES= ../trio/regression.c
endif

LDADD = libbogofilter.a @LIBOBJS@

# what to build that from
libbogofilter_a_SOURCES= \
	common.h system.h \
	globals.h globals.c \
	directories.c version.c \
	base64.h base64.c \
	bf_exit.c \
	bogoconfig.h bogoconfig.c \
	bogoreader.h bogoreader.c \
	bool.h bool.c \
	buff.h buff.c \
	charset.h charset.c \
	collect.h collect.c \
	configfile.h configfile.c \
	datastore.h datastore.c db_handle_props.h \
	debug.h debug.c \
	degen.h degen.c \
	error.h error.c \
	fgetsl.h fgetsl.c \
	find_home.h find_home.c find_home_user.c find_home_tildeexpand.c \
	format.h format.c \
	lexer.h lexer.c lexer_v3.l \
	maint.h maint.c \
	mime.h mime.c \
	msgcounts.h msgcounts.c \
	paths.h paths.c \
	qp.h qp.c \
	register.h register.c \
	rstats.h rstats.c \
	swap.h swap_32bit.c\
	textblock.h textblock.c \
	token.h token.c \
	uudecode.h uudecode.c \
	word.h word.c \
	wordhash.h wordhash.c wordlists.h wordlists.c \
	xmalloc.h xcalloc.c xmalloc.c xmem_error.c xrealloc.c \
	xmemrchr.h xmemrchr.c \
	xstrdup.h xstrdup.c \
	xstrlcat.h xstrlcat.c \
	xstrlcpy.h xstrlcpy.c \
	$(MEMDEBUG_SRC) \
	dcdflib/src/dcdflib.c dcdflib/src/ipmpar.c \
	xatox.h xatof.c xatoi.c \
	datastore_db.h

if ENABLE_TDB_DATASTORE
libbogofilter_a_SOURCES += datastore_tdb.c
else
if ENABLE_QDBM_DATASTORE
libbogofilter_a_SOURCES += datastore_qdbm.c
else
libbogofilter_a_SOURCES += datastore_db.c
endif
endif

CLEANFILES=version.c directories.c

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c \
		     method.c $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		     common.h method.h
bogofilter_static_SOURCES = $(bogofilter_SOURCES)
bogofilter_static_LDFLAGS = $(STATICLDFLAGS)

bogolexer_static_SOURCES = bogolexer.c
bogolexer_static_LDFLAGS = $(STATICLDFLAGS)

bogoutil_static_SOURCES = bogoutil.c
bogoutil_static_LDFLAGS = $(STATICLDFLAGS)

debugtest_SOURCES = debug.main.c

wordhash_SOURCES = wordhash.main.c

find_home_test_SOURCES = find_home.test.c

fgetsl_test_SOURCES = fgetsl.c
fgetsl_test_CFLAGS= -DMAIN

# what to distribute
EXTRA_DIST = \
	     bogoupgrade \
	     version.sh \
	     strlcat.3 strlcpy.3
#
version.c:	version.sh
	$(srcdir)/version.sh $(top_srcdir) >"$@.new" && mv "$@.new" "$@" \
	    || rm -f "$@"
#
directories.c: ../config.status
	echo "const char *const system_config_file = \"$(SYSCONFDIR)/bogofilter.cf\";" >"$@" \
	    || rm -f "$@"
#
.l.c:
	flex -o$@ $<
