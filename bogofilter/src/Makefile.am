# vim:tw=0
# $Id$

SUBDIRS = . tests
SYSCONFDIR = @sysconfdir@

PERL = @PERL@

if ENABLE_DEPRECATED_CODE
GRAHAM_SRC = graham.c graham.h
DEGEN = degen.h degen.c
endif

FISHER_SRC = fisher.c fisher.h
ROBINSON_SRC = robinson.c robinson.h

if ENABLE_MEMDEBUG
MEMDEBUG_SRC = memdebug.c memdebug.h
endif

noinst_LIBRARIES = libbogofilter.a

if NEED_GSL
libbf_gsl_a_CFLAGS=$(AM_CFLAGS) -DRENAMEFILES
libbf_gsl_a_SOURCES= \
../gsl/cdf/chisq.c ../gsl/cdf/gamma.c \
../gsl/specfunc/gamma_inc.c ../gsl/err/error.c \
../gsl/sys/infnan.c ../gsl/sys/fdiv.c \
../gsl/specfunc/exp.c ../gsl/specfunc/trig.c \
../gsl/specfunc/log.c ../gsl/specfunc/psi.c \
../gsl/specfunc/zeta.c ../gsl/specfunc/elementary.c \
../gsl/err/stream.c ../gsl/specfunc/expint.c \
../gsl/specfunc/erfc.c ../gsl/sys/coerce.c \
../gsl/sys/pow_int.c ../gsl/sys/minmax.c
noinst_LIBRARIES += libbf_gsl.a
endif

AM_CPPFLAGS = -I$(top_srcdir)/trio -I$(top_srcdir) @GSL_CFLAGS@
AM_CFLAGS = -DBOGOFILTER

if ENABLE_MEMDEBUG
AM_CFLAGS += -DENABLE_MEMDEBUG
endif

# what to build
bin_PROGRAMS = bogofilter bogoutil bogolexer bogotune
bin_SCRIPTS = bogoupgrade
if ENABLE_STATIC
bin_PROGRAMS += bogofilter_static bogoutil_static bogolexer_static bogotune_static
endif

check_PROGRAMS = debugtest configtest wordhash find_home.test \
		 bogowordfreq fgetsl.test

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
bogofilter_LDADD = $(LDADD) $(LIBDB) $(GSL_LIBS)
bogoutil_LDADD = $(LDADD) $(LIBDB)
configtest_LDADD = $(LDADD) $(LIBDB)

# what to build that from
version_sources= \
	common.h system.h \
	globals.h globals.c \
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
	$(DEGEN) \
	error.h error.c \
	fgetsl.h fgetsl.c \
	find_home.h find_home.c find_home_user.c find_home_tildeexpand.c \
	format.h format.c \
	lexer.h lexer.c lexer_v3.l \
	maint.h maint.c \
	mime.h mime.c \
	msgcounts.h msgcounts.c \
	passthrough.h passthrough.c \
	paths.h paths.c \
	prob.h prob.c \
	qp.h qp.c \
	register.h register.c \
	robx.h robx.c \
	rstats.h rstats.c \
	swap.h swap_32bit.c system.c \
	textblock.h textblock.c \
	token.h token.c \
	transaction.h transaction.c \
	uudecode.h uudecode.c \
	word.h word.c \
	wordhash.h wordhash.c wordlists.h wordlists.c \
	wordlists_base.h wordlists_base.c \
	xmalloc.h xcalloc.c xmalloc.c xmem_error.c xrealloc.c \
	xmemrchr.h xmemrchr.c \
	xstrdup.h xstrdup.c \
	xstrlcat.h xstrlcat.c \
	xstrlcpy.h xstrlcpy.c \
	$(MEMDEBUG_SRC) \
	xatox.h xatof.c xatoi.c \
	datastore_db.h \
	mysetvbuf.c mysetvbuf.h

BUILT_SOURCES=	version.c

libbogofilter_a_SOURCES = \
	directories.c version.c \
	$(version_sources)

if ENABLE_TDB_DATASTORE
datastore_SOURCE = datastore_tdb.c
else
if ENABLE_QDBM_DATASTORE
datastore_SOURCE = datastore_qdbm.c
else
datastore_SOURCE = datastore_db.c
endif
endif

datastore_OBJECT = $(datastore_SOURCE:.c=.o)

libbogofilter_a_SOURCES += $(datastore_SOURCE)
EXTRA_libbogofilter_a_SOURCES = datastore_db.c datastore_qdbm.c datastore_tdb.c

CLEANFILES=version.c directories.c bogoupgrade

bogofilter_SOURCES = bogofilter.c bogofilter.h main.c \
		     method.c $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		     common.h method.h
bogofilter_static_SOURCES = $(bogofilter_SOURCES)
bogofilter_static_LDFLAGS = $(STATICLDFLAGS)
bogofilter_static_LDADD = $(LDADD) $(STATIC_DB) $(GSL_LIBS)

bogolexer_static_SOURCES = bogolexer.c
bogolexer_static_LDFLAGS = $(STATICLDFLAGS)

bogoutil_SOURCES = bogoutil.c bogohist.c
bogoutil_static_SOURCES = bogoutil.c bogohist.c
bogoutil_static_LDFLAGS = $(STATICLDFLAGS)
bogoutil_static_LDADD = $(LDADD) $(STATIC_DB)

bogotune_SOURCES = bogotune.c bogotune.h \
		   tunelist.c tunelist.h \
		   method.c $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		   common.h method.h
bogotune_LDADD = $(LDADD) $(LIBDB) $(GSL_LIBS)

bogotune_static_SOURCES = bogotune.c bogotune.h \
		   tunelist.c tunelist.h \
		   method.c $(GRAHAM_SRC) $(ROBINSON_SRC) $(FISHER_SRC) \
		   common.h method.h
bogotune_static_LDFLAGS = $(STATICLDFLAGS)
bogotune_static_LDADD= $(LDADD) $(STATIC_DB) $(GSL_LIBS)

debugtest_SOURCES = debug.main.c

wordhash_SOURCES = wordhash.main.c

find_home_test_SOURCES = find_home.test.c

fgetsl_test_SOURCES = fgetsl.c
fgetsl_test_CFLAGS= -DMAIN

# what to distribute
EXTRA_DIST = \
	     bogoupgrade.in \
	     version.sh \
	     strlcat.3 strlcpy.3
#
version.c: version.sh ../configure.ac Makefile.am	\
	$(version_sources)				\
	$(bogofilter_SOURCES)				\
	$(bogolexer_SOURCES)				\
	$(bogoutil_SOURCES)				\
	$(bogotune_SOURCES)

	$(SHELL) $(srcdir)/version.sh $(top_srcdir) >"$@.new" && mv "$@.new" "$@" \
	    || rm -f "$@"
#
directories.c: ../config.status
	echo "const char *const system_config_file = \"$(SYSCONFDIR)/bogofilter.cf\";" >"$@" \
	    || rm -f "$@"
#
.l.c:
	flex -o$@.new $< && \
	sed 's/yy_rule_linenum\[yy_act\]/(long int)yy_rule_linenum[yy_act]/; s/accepting rule at line %d/accepting rule at line %ld/' <$@.new >$@ && rm -f $@.new || \
	rm -f $@ $@.new
#
$(datastore_OBJECT): Makefile $(datastore_SOURCE)
#
bogoupgrade: bogoupgrade.in Makefile
	rm -f $@
	sed 's}[@]PERL@}$(PERL)}' <$(srcdir)/bogoupgrade.in >$@ || rm -f $@
	chmod a=rx bogoupgrade

#splint - weak
splint.check:
	splint -weak `	ls \
	$(version_sources)	\
	$(bogofilter_SOURCES)	\
	$(bogolexer_SOURCES)	\
	$(bogoutil_SOURCES)	\
	$(bogotune_SOURCES) | sort -u | egrep -v "(lexer_v3.l|db_handle_props.h)"` > splint.tmp 
	grep ^F splint.tmp
	wc -l splint.tmp
