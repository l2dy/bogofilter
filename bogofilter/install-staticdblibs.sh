#! /bin/sh

# install-staticdblibs.sh -- shell script to fetch, build and install
#                            the static libraries bogofilter links against,
#                            to ease building of portable RPMs.

# (C) Copyright 2005,2006,2007  Matthias Andree <matthias.andree@gmx.de>
# All rights reserved.
#
# This script can be redistributed solely under the terms of the
# GNU General Public License v3.

# this script requires these in the PATH:
# ---------------------------------------
# sudo          - must be configured properly for the user running this script
# wget or curl  - must be installed
# md5sum or md5 - must be installed
# gunzip        - must be installed
# tar           - must be installed
# make          - must be installed

# this script will:
# -----------------
# download  6 MB from the network
# require  44 MB in /var/tmp
# require   3 MB in /opt

# _fetch_xxx URL
_fetch_curl() {
    curl -q -s -S -L -O "$1"
}

_fetch_wget() {
    wget -nc -nv "$1"
}

# fetch URL
# fetch a file with curl or wget
fetch() {
    if command -v >/dev/null 2>&1 curl ; then
	_fetch_curl "$@"
    else
	_fetch_wget "$@"
    fi
}

# _md5_file FILE
# check MD5 sum of a single file
_md5_file() {
    if command -v >/dev/null 2>&1 md5sum
    then
	md5sum "$1" | cut -f1 -d" "
    else if command -v >/dev/null 2>&1 md5
    then
	md5 -q "$1"
    else
	echo >&2 "Cannot calculate md5 hash, no program found."
	exit 1
    fi
    fi
}

# fetch file and check MD5 sum
want() {
    f=${1##*/}
    if test ! -f ${f} || test "$(_md5_file ${f})" != "$2" ; then
	rm -f ${f}
	echo "fetching $1 -> ${f}"
	fetch "$1"
    else
	echo "${f} was already downloaded, reusing it"
    fi
    if test "$(_md5_file ${1##*/})" = "$2" ; then
	echo "${f} MD5 checksum OK"
    else
	echo "${f} MD5 checksum FAIL, aborting." >&2
	exit 1
    fi
}

set -e

cd ${TMPDIR:=/var/tmp}

dbdir=db-4.2.52
dbpfx=/opt/db-4.2-lean
sqdir=sqlite-3.4.1
sqpfx=/opt/sqlite-3-lean

### download SleepyCat DB 4.2.52 and patches
source=oracle.com
build_db=0
dbpatches="1 2 3 4 5"
checklib=$dbpfx/lib/libdb.a
if test ! -f $checklib ; then
    case $source in
	oracle.com)
	URL1=http://download.oracle.com/berkeley-db
	URL2=http://www.oracle.com/technology/products/berkeley-db/db/
	want $URL1/db-4.2.52.tar.gz             8b5cff6eb83972afdd8e0b821703c33c
	want $URL2/update/4.2.52/patch.4.2.52.1 1227f5f9ff43d48b5b1759e113a1c2d7
	want $URL2/update/4.2.52/patch.4.2.52.2 3da7efd8d29919a9113e2f6f5166f5b7
	want $URL2/update/4.2.52/patch.4.2.52.3 0bf9ebbe852652bed433e522928d40ec
	want $URL2/update/4.2.52/patch.4.2.52.4 9cfeff4dce0c11372c0b04b134f8faef
	want $URL2/update/4.2.52/patch.4.2.52.5 99836f962361da8936219cc193edc7ed
	;;
    esac
    build_db=1
else
    echo "$checklib already exists, not building Berkeley DB."
fi

### download SQLite 3.3.5
# Info: the objdump test fixes up the effects of a bug
# in an earlier version of this script, which built
# a sqlite 3.2.8 version that required GLIBC_2.3.
source=sqlite.org
build_sqlite=0
checklib=$sqpfx/lib/libsqlite3.a
if test ! -f $checklib || \
    objdump -t /opt/sqlite-3-lean/lib/libsqlite3.a \
	| grep -q __ctype_b_loc ; then
    case $source in
    sqlite.org)
	URL=http://www.sqlite.org ;;
    bogofilter.org)
	URL=ftp://ftp.bogofilter.org/pub/outgoing/tools/SQLite ;;
    esac
    want $URL/sqlite-3.4.1.tar.gz 0f06955b18da295fecb62d4bf9ded3c6
    build_sqlite=1
else
    echo "$checklib already exists, not building SQLite3."
fi

# build DB 4.2
if test $build_db = 1 ; then
    rm -rf $dbdir
    gunzip -c -d $dbdir.tar.gz | tar xf -
    for N in $dbpatches ; do
	if [ -f patch.4.2.52.$N ] ; then
	    patch -s -d $dbdir -p0 <patch.4.2.52.$N
	fi
    done
    echo "installing $dbdir into $dbpfx"
    cd $dbdir/build_unix
    env CPPFLAGS=-D__NO_CTYPE ../dist/configure \
	--prefix=$dbpfx --silent \
	--with-mutex=x86/gcc-assembly \
	--disable-cxx --disable-shared \
	--disable-queue --disable-replication \
	--disable-hash  --disable-cryptography
    make -s
    sudo make -s install_setup install_include install_lib
    # fix permissions:
    sudo chown -R 0:0 $dbpfx
    sudo chmod -R a-w $dbpfx
    # save some kBytes:
    sudo ln -f $dbpfx/lib/libdb-4.2.a $dbpfx/lib/libdb.a
    echo "$dbdir was installed into $dbpfx"
    cd - >/dev/null
    rm -rf $dbdir
fi

# build SQLite 3
if test $build_sqlite = 1 ; then
    rm -rf build-$sqdir $sqdir
    gunzip -cd $sqdir.tar.gz | tar xf -
    set -e
    echo "installing $sqdir"
    mkdir -p build-$sqdir
    cd build-$sqdir
    env CFLAGS="-O2 -D__NO_CTYPE" ../$sqdir/configure \
	--prefix=$sqpfx --silent \
	--disable-shared --disable-tcl \
	--disable-threadsafe
    make -s
    sudo make -s install
    # fix permissions:
    sudo chown -R 0:0 $sqpfx
    sudo chmod -R a-w $sqpfx
    echo "$sqdir was installed into $sqpfx"
    cd - >/dev/null
    rm -rf build-$sqdir $sqdir
fi

rm -f $dbdir.tar.gz $sqdir.tar.gz patch.4.2.52.?
