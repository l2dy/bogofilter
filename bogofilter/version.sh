#!/bin/bash

#	version.sh
#
#	create version.h to define BOGOFILTER_VERSION
#
#	if VERSION in config.h does not contain 'cvs', use its value.
#	if it does contain 'cvs', append the a 4 digit date to it.
#	the 4 digit date is the newer of the two $Id: dates
#	in main.c and bogofilter.c
#
#	#define BOGOFILTER_VERSION "0.7.5"
# or:
#	#define BOGOFILTER_VERSION "0.7.5.cvs.1012"

VERSION=`grep define.VERSION config.h | awk '{print $3}' | tr -d '"'`

echo $VERSION | egrep "[a-z][a-z][a-z]$" >/dev/null

srcdir=$1
shift

if [ $? == 0 ]; then
   DATE="00000000"
   for file in $* ; do
       HEAD=`head -1 $srcdir/$file | awk '{print $5}' | tr -d "/"`
       if [ $HEAD -gt $DATE  ] ; then
	   DATE="$HEAD"
       fi
   done
   VERSION="$VERSION.$DATE"
fi

echo "#undef  VERSION"
echo "#define VERSION \"$VERSION\""
