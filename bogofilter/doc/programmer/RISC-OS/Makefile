# Makefile for the RISC OS version of bogofilter


# Define which compiler to use:

CC = cc
#CC = gcc

# Define the database backend to use:

#DB=qdbm
DB=sqlite3

# Define whether you want iconv or not:
ENABLE_ICONV=-DENABLE_ICONV=1 -DENABLE_UNICODE=1

# You might have to change the paths in the DBLIB and DBINCLUDE variables
# to suit your configuration.

ifeq ($(DB),qdbm)
DBINCLUDE = ^.^.qdbm
DBLIB = ^.^.qdbm.libqdbm
else
ifeq ($(DB),sqlite3)
DBINCLUDE = ^.^.SQLite3Lib
DBLIB = ^.^.SQLite3Lib.SQLiteLib3_0_8
else
error No database backend selected!
endif
endif


# If you want memory debugging, comment out the following two lines.

#DMEMDEBUG = -DENABLE_MEMDEBUG=1
#OMEMDEBUG = o.memdebug


#########################################
# DO NOT EDIT ANYTHING BELOW THIS LINE! #
#########################################

ifeq (${CC},cc)
MAKE = amu
CC = cc
LD = link
AR = libfile
DEPEND = -depend !Depend
CC_FLAGS = -Wdp -fa -throwback -Otime -I@,^,^.^,$(DBINCLUDE),UnixLib: -JUnixLib: -D__riscos__ -DHAVE_CONFIG_H $(DMEMDEBUG)
UNIXLIB = UnixLib:o.UnixLib
else
ifeq (${CC},gcc)
MAKE = make
CC = gcc
LD = gcc
AR = ar
CC_FLAGS = -mthrowback -O3 -I@ -I^ -I^.^ -I$(DBINCLUDE) -D__riscos__ -DHAVE_CONFIG_H $(DMEMDEBUG)
else
error No compiler selected!
endif
endif

LIBGSL = ^.gsl.libgsl

COMMON_OBJS = o.base64 \
	o.bf_exit \
	o.bogoconfig \
	o.bogofilter \
	o.bogohist \
	o.bogomain \
	o.bogoreader \
	o.bool \
	o.buff \
	o.charset \
	o.collect \
	o.configfile \
	o.convert_charset \
	o.datastore \
	o.datastore_dummies \
	o.datastore_opthelp_dummies \
	o.datastore_txn_dummies \
	o.debug \
	o.error \
	o.fgetsl \
	o.find_home \
	o.find_home_tildeexpand \
	o.find_home_user \
	o.format \
	o.globals \
	o.lexer \
	o.lexer_v3 \
	o.maint \
	o.memcmp \
	o.memstr \
	o.mime \
	o.msgcounts \
	o.mxcat \
	o.mysetvbuf \
	o.passthrough \
	o.paths \
	o.prob \
	o.qp \
	o.rand_sleep \
	o.register \
	o.robx \
	o.rstats \
	o.score \
	o.sighandler \
	o.strlcat \
	o.strlcpy \
	o.swap_32bit \
	o.system \
	o.textblock \
	o.token \
	o.transaction \
	o.tunelist \
	o.uudecode \
	o.version \
	o.word \
	o.wordhash \
	o.wordlists \
	o.wordlists_base \
	o.xatof \
	o.xatoi \
	o.xcalloc \
	o.xmalloc \
	o.xmemrchr \
	o.xmem_error \
	o.xrealloc \
	o.xstrdup \
	o.xstrlcat \
	o.xstrlcpy \
	$(OMEMDEBUG)

ifdef ENABLE_ICONV
COMMON_OBJS += o.convert_unicode \
	o.iconvert
CC_FLAGS += $(ENABLE_ICONV)
endif

ifeq ($(DB),qdbm)
COMMON_OBJS += o.datastore_qdbm \
	o.datastore_qdbm_cmpkey
CC_FLAGS += -DENABLE_QDBM_DATASTORE=1
else
ifeq ($(DB),sqlite3)
COMMON_OBJS += o.datastore_sqlite
CC_FLAGS += -DENABLE_SQLITE_DATASTORE=1
else
error No database backend selected!
endif
endif

BOGOFILTER_OBJS = o.main

BOGOLEXER_OBJS = o.bogolexer

BOGOTUNE_OBJS = o.bogotune

BOGOUTIL_OBJS = o.bogoutil

CONFIGTEST_OBJS = o.configtest

BOGOQDBMUPGRADE_OBJS = o.bogoQDBMupgrade

.PHONY:	all dist install clean

.INIT:
	@cdir o

## Rule Patterns ##

.SUFFIXES:	.c .o

.c.o:
	$(CC) $(CC_FLAGS) $(DEPEND) -c -o $@ $<

# Static dependencies:

ifeq ($(DB),qdbm)
all: bogofilter bogolexer bogotune bogoutil configtest bogoQDBMupgrade
else
ifeq ($(DB),sqlite3)
all: bogofilter bogolexer bogotune bogoutil configtest
else
error No database backend selected!
endif
endif

dist: install
	-cdir <Bogofilter$Dir>.doc
	-copy ^.AUTHORS <Bogofilter$Dir>.doc.* ~CF~V
	-copy ^.COPYING <Bogofilter$Dir>.doc.* ~CF~V
	-copy ^.NEWS <Bogofilter$Dir>.doc.* ~CF~V
	-copy ^.doc.bogofilter-faq/html <Bogofilter$Dir>.doc.* ~CF~V
	-copy ^.doc.bogofilter/htmlin <Bogofilter$Dir>.doc.bogofilter/html ~CF~V
	-copy ^.doc.bogolexer/htmlin <Bogofilter$Dir>.doc.bogolexer/html ~CF~V
	-copy ^.doc.bogotune/htmlin <Bogofilter$Dir>.doc.bogotune/html ~CF~V
	-copy ^.doc.bogoutil/htmlin <Bogofilter$Dir>.doc.bogoutil/html ~CF~V
	-settype <Bogofilter$Dir>.doc.AUTHORS Text
	-settype <Bogofilter$Dir>.doc.COPYING Text
	-settype <Bogofilter$Dir>.doc.NEWS Text
	-settype <Bogofilter$Dir>.doc.bogofilter-faq/html HTML
	-settype <Bogofilter$Dir>.doc.bogofilter/html HTML
	-settype <Bogofilter$Dir>.doc.bogolexer/html HTML
	-settype <Bogofilter$Dir>.doc.bogotune/html HTML
	-settype <Bogofilter$Dir>.doc.bogoutil/html HTML

install: bogofilter bogolexer bogotune bogoutil configtest
	-copy bogofilter <Bogofilter$Dir>.bin.* ~CF~V
	-copy bogolexer <Bogofilter$Dir>.bin.* ~CF~V
	-copy bogotune <Bogofilter$Dir>.bin.* ~CF~V
	-copy bogoutil <Bogofilter$Dir>.bin.* ~CF~V
	-copy configtest <Bogofilter$Dir>.bin.* ~CF~V
	-copy bogoQDBMupgrade <Bogofilter$Dir>.contrib.* ~CF~V

libbogo: $(COMMON_OBJS)
	$(AR) $(AR_FLAGS) -c -o $@ $(COMMON_OBJS)

$(LIBGSL):
	dir ^.gsl
	$(MAKE)
	dir ^.src

bogofilter: libbogo $(LIBGSL) $(BOGOFILTER_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOFILTER_OBJS) libbogo $(LIBGSL) $(DBLIB) $(UNIXLIB)

bogolexer: libbogo $(BOGOLEXER_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOLEXER_OBJS) libbogo $(DBLIB) $(UNIXLIB)

bogotune: libbogo $(LIBGSL) $(BOGOTUNE_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOTUNE_OBJS) libbogo $(LIBGSL) $(DBLIB) $(UNIXLIB)

bogoutil: libbogo $(LIBGSL) $(BOGOUTIL_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOUTIL_OBJS) libbogo $(LIBGSL) $(DBLIB) $(UNIXLIB)

configtest: libbogo $(CONFIGTEST_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(CONFIGTEST_OBJS) libbogo $(DBLIB) $(UNIXLIB)

bogoQDBMupgrade: libbogo $(BOGOQDBMUPGRADE_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOQDBMUPGRADE_OBJS) libbogo $(DBLIB) $(UNIXLIB)

clean:
	-ifthere bogofilter then wipe bogofilter ~CF~V
	-ifthere bogolexer then wipe bogolexer ~CF~V
	-ifthere bogotune then wipe bogotune ~CF~V
	-ifthere bogoutil then wipe bogoutil ~CF~V
	-ifthere configtest then wipe configtest ~CF~V
	-ifthere bogoQDBMupgrade then wipe bogoQDBMupgrade ~CF~V
	-ifthere libbogo then wipe libbogo ~CF~V
	-ifthere o.* then wipe o.* ~CF~V

# Dynamic dependencies:
