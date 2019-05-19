#! /bin/sh
set -eux
rm -f bogofilter.fi bogofilter.fo bogofilter.svn
make
exec ./post-conversion-cleanup.sh
