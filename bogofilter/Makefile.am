# $Id$ 
# $Log$
# Revision 1.2  2002/09/16 19:24:53  m-a
# Add missing .h files and list man1_MANS in EXTRA_DIST
# so that 'make distcheck' works.
#
# Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
# 0.7.3 Base Source
#
AUTOMAKE_OPTIONS = foreign # we don't have ChangeLog
bin_PROGRAMS = bogofilter
bogofilter_SOURCES = bogofilter.c bogofilter.h lock.c lock.h main.c lexer_l.l
man1_MANS = bogofilter.1
EXTRA_DIST = $(man1_MANS)
