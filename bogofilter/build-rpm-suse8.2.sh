#! /bin/bash

if [ "x$1" = x ] ; then
    echo >&2 "Usage: `basename $0` path-to-bogofilter.tar.gz"
    exit 1
fi

export STATIC_DB=/usr/local/BerkeleyDB.4.1/lib/libdb.a
LD_LIBRARY_PATH=/usr/local/BerkeleyDB.4.1/lib \
CPPFLAGS=-I/usr/local/BerkeleyDB.4.1/include \
LDFLAGS=-L/usr/local/BerkeleyDB.4.1/lib \
rpm --define 'bogostatic 1' -tb --sign --target=i586 \
"$@"
