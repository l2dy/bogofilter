# Makefile for the RISC OS version of bogofilter


# Define which compiler to use:

CC = cc
#CC = gcc


# Define exactly one of the following three in order to use that database:

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
CC = cc
LD = link
DEPEND = -depend !Depend
CC_FLAGS = -Wpc -zo -fw -throwback -Otime -I@,dcdflib.src,$(DBINCLUDE),Unix: -JUnix: -D__riscos__ -DHAVE_CONFIG_H
UNIXLIB = Unix:o.UnixLib
else
ifeq (${CC},gcc)
CC = gcc
LD = gcc
CC_FLAGS = -mthrowback -O3 -I. -Idcdflib/src -I$(DBINCLUDE) -D__riscos__ -DHAVE_CONFIG_H
else
# No other compiler supported!
endif
endif

BOGOFILTER_OBJS = o.base64 \
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
	o.paths \
	o.qp \
	o.register \
	o.robinson \
	o.rstats \
	o.strlcat \
	o.strlcpy \
	o.swap_32bit \
	o.textblock \
	o.token \
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
	o.xstrlcpy \
	dcdflib.src.o.dcdflib \
	dcdflib.src.o.ipmpar
	
BOGOLEXER_OBJS = o.base64 \
	o.bogoreader \
	o.bool \
	o.buff \
	o.charset \
	o.configfile \
	o.datastore \
	$(DBSTORE) \
	o.debug \
	o.directories \
	o.error \
	o.fgetsl \
	o.find_home \
	o.find_home_tildeexpand \
	o.find_home_user \
	o.globals \
	o.lexer \
	o.lexer_v3 \
	o.maint \
	o.memdebug \
	o.mime \
	o.msgcounts \
	o.paths \
	o.qp \
	o.strlcat \
	o.strlcpy \
	o.swap_32bit \
	o.textblock \
	o.token \
	o.uudecode \
	o.version \
	o.word \
	o.wordlists \
	o.xatoi \
	o.xatof \
	o.xcalloc \
	o.xmalloc \
	o.xmemrchr \
	o.xmem_error \
	o.xrealloc \
	o.xstrdup

BOGOUTIL_OBJS = o.buff \
	o.datastore \
	$(DBSTORE) \
	o.debug \
	o.error \
	o.fgetsl \
	o.find_home \
	o.find_home_tildeexpand \
	o.find_home_user \
	o.globals \
	o.maint \
	o.memdebug \
	o.paths \
	o.strlcat \
	o.strlcpy \
	o.swap_32bit \
	o.version \
	o.word \
	o.wordlists \
	o.xcalloc \
	o.xmalloc \
	o.xmem_error \
	o.xrealloc \
	o.xstrdup

BOGOWORDFREQ_OBJS = o.base64 \
	o.bogoreader \
	o.buff \
	o.charset \
	o.collect \
	o.debug \
	o.error \
	o.fgetsl \
	o.globals \
	o.lexer \
	o.lexer_v3 \
	o.memdebug \
	o.mime \
	o.msgcounts \
	o.qp \
	o.strlcat \
	o.strlcpy \
	o.textblock \
	o.token \
	o.uudecode \
	o.word \
	o.wordhash \
	o.xcalloc \
	o.xmalloc \
	o.xmemrchr \
	o.xmem_error \
	o.xrealloc \
	o.xstrdup

CONFIGTEST_OBJS = o.base64 \
	o.bogoreader \
	o.bool \
	o.buff \
	o.charset \
	o.datastore \
	$(DBSTORE) \
	o.debug \
	o.directories \
	o.error \
	o.fgetsl \
	o.find_home \
	o.find_home_tildeexpand \
	o.find_home_user \
	o.globals \
	o.lexer \
	o.lexer_v3 \
	o.maint \
	o.memdebug \
	o.mime \
	o.msgcounts \
	o.paths \
	o.qp \
	o.strlcat \
	o.strlcpy \
	o.swap_32bit \
	o.textblock \
	o.token \
	o.uudecode \
	o.version \
	o.word \
	o.wordlists \
	o.xatoi \
	o.xatof \
	o.xcalloc \
	o.xmalloc \
	o.xmemrchr \
	o.xmem_error \
	o.xrealloc \
	o.xstrdup

.INIT:
	@cdir o
	@cdir dcdflib.src.o

## Rule Patterns ##

.SUFFIXES:	.c .o

.c.o:
	$(CC) $(CC_FLAGS) $(DEPEND) -c -o $@ $<

# Static dependencies:

all: bogofilter bogolexer bogoutil bogowordfreq # configtest

bogofilter: $(BOGOFILTER_OBJS) $(DBLIB) o.main
	$(LD) $(LD_FLAGS) -o bogofilter o.main $(BOGOFILTER_OBJS) $(DBLIB) $(UNIXLIB)

bogolexer: $(BOGOLEXER_OBJS) $(DBLIB) o.bogolexer
	$(LD) $(LD_FLAGS) -o bogolexer o.bogolexer $(BOGOLEXER_OBJS) $(DBLIB) $(UNIXLIB)

bogoutil: $(BOGOUTIL_OBJS) $(DBLIB) o.bogoutil
	$(LD) $(LD_FLAGS) -o bogoutil o.bogoutil $(BOGOUTIL_OBJS) $(DBLIB) $(UNIXLIB)

bogowordfreq: $(BOGOWORDFREQ_OBJS) $(DBLIB) o.bogowordfreq
	$(LD) $(LD_FLAGS) -o bogowordfreq o.bogowordfreq $(BOGOWORDFREQ_OBJS) $(DBLIB) $(UNIXLIB)

configtest: $(CONFIGTEST_OBJS) $(DBLIB) o.configtest
	$(LD) $(LD_FLAGS) -o configtest o.configtest $(CONFIGTEST_OBJS) $(DBLIB) $(UNIXLIB)

clean:
	-ifthere bogofilter then wipe bogofilter ~CFR~V
	-ifthere bogolexer then wipe bogolexer ~CFR~V
	-ifthere bogoutil then wipe bogoutil ~CFR~V
	-ifthere bogowordfreq then wipe bogowordfreq ~CFR~V
	-ifthere configtest then wipe configtest ~CFR~V
	-ifthere o.* then wipe o.* ~CFR~V
	-ifthere dcdflib.src.o.* then wipe dcdflib.src.o.* ~CFR~V

# Dynamic dependencies:
