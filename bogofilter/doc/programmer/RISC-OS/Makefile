# Makefile for the RISC OS version of bogofilter


# Define which compiler to use:

CC = cc
#CC = gcc


# Define exactly one of the following two in order to use that database:

USE_DB = qdbm
#USE_DB = tdb


# You might have to change the paths in the DBLIB and DBINCLUDE variables
# to suit your configuration.

ifeq (${USE_DB},qdbm)
DBINCLUDE = ^.^.qdbm
DBLIB = ^.^.qdbm.libqdbm
DBSTORE = o.datastore_qdbm
else
ifeq (${USE_DB},tdb)
DBINCLUDE = ^.^.tdb
DBLIB = ^.^.tdb.libtdb
DBSTORE = o.datastore_tdb
else
# No other database supported!
endif
endif

#########################################
# DO NOT EDIT ANYTHING BELOW THIS LINE! #
#########################################

ifeq (${CC},cc)
MAKE = amu
CC = cc
LD = link
AR = libfile
DEPEND = -depend !Depend
CC_FLAGS = -Wpc -zo -fw -throwback -Otime -I@,^,^.^,$(DBINCLUDE),Unix: -JUnix: -D__riscos__ -DHAVE_CONFIG_H
UNIXLIB = Unix:o.UnixLib
else
ifeq (${CC},gcc)
MAKE = make
CC = gcc
LD = gcc
AR = ar
CC_FLAGS = -mthrowback -O3 -I@ -I^ -I^.^ -I$(DBINCLUDE) -D__riscos__ -DHAVE_CONFIG_H
else
# No other compiler supported!
endif
endif

LIBGSL = ^.gsl.libgsl

COMMON_OBJS = o.base64 \
	o.bf_exit \
	o.bogoconfig \
	o.bogofilter \
	o.bogoreader \
	o.bool \
	o.buff \
	o.charset \
	o.collect \
	o.configfile \
	o.datastore \
	$(DBSTORE) \
	o.debug \
	o.degen \
	o.directories \
	o.error \
	o.fgetsl \
	o.find_home \
	o.find_home_tildeexpand \
	o.find_home_user \
	o.fisher \
	o.format \
	o.globals \
	o.graham \
	o.lexer \
	o.lexer_v3 \
	o.maint \
	o.memcmp \
	o.memdebug \
	o.method \
	o.mime \
	o.msgcounts \
	o.passthrough \
	o.paths \
	o.qp \
	o.register \
	o.robinson \
	o.rstats \
	o.strlcat \
	o.strlcpy \
	o.swap_32bit \
	o.system \
	o.textblock \
	o.token \
	o.transaction \
	o.uudecode \
	o.version \
	o.word \
	o.wordhash \
	o.wordlists \
	o.xatof \
	o.xatoi \
	o.xcalloc \
	o.xmalloc \
	o.xmemrchr \
	o.xmem_error \
	o.xrealloc \
	o.xstrdup \
	o.xstrlcat \
	o.xstrlcpy

BOGOFILTER_OBJS = o.main
	
BOGOLEXER_OBJS = o.bogolexer

BOGOUTIL_OBJS = o.bogoutil

BOGOWORDFREQ_OBJS = o.bogowordfreq

CONFIGTEST_OBJS = o.configtest

.INIT:
	@cdir o

## Rule Patterns ##

.SUFFIXES:	.c .o

.c.o:
	$(CC) $(CC_FLAGS) $(DEPEND) -c -o $@ $<

# Static dependencies:

all: bogofilter bogolexer bogoutil bogowordfreq configtest

install: bogofilter bogolexer bogoutil bogowordfreq configtest
	-copy bogofilter <Bogofilter$Dir>.bogofilter ~CF~N~V
	-copy bogolexer <Bogofilter$Dir>.bogolexer ~CF~N~V
	-copy bogoutil <Bogofilter$Dir>.bogoutil ~CF~N~V
	-copy bogowordfreq <Bogofilter$Dir>.bogowordfreq ~CF~N~V
	-copy configtest <Bogofilter$Dir>.configtest ~CF~N~V

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

bogoutil: libbogo $(BOGOUTIL_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOUTIL_OBJS) libbogo $(DBLIB) $(UNIXLIB)

bogowordfreq: libbogo $(BOGOWORDFREQ_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(BOGOWORDFREQ_OBJS) libbogo $(DBLIB) $(UNIXLIB)

configtest: libbogo $(CONFIGTEST_OBJS) $(DBLIB)
	$(LD) $(LD_FLAGS) -o $@ $(CONFIGTEST_OBJS) libbogo $(DBLIB) $(UNIXLIB)

clean:
	-ifthere bogofilter then wipe bogofilter ~CFR~V
	-ifthere bogolexer then wipe bogolexer ~CFR~V
	-ifthere bogoutil then wipe bogoutil ~CFR~V
	-ifthere bogowordfreq then wipe bogowordfreq ~CFR~V
	-ifthere configtest then wipe configtest ~CFR~V
	-ifthere libbogo then wipe libbogo ~CFR~V
	-ifthere o.* then wipe o.* ~CFR~V

# Dynamic dependencies:
