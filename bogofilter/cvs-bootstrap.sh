#! /bin/sh

###############################################################################
# cvs-bootstrap.sh -- rebuild auto* files after fresh CVS checkout
# (C) 2002 by Matthias Andree
#
# This program tries to work around Red Hat braindeadness installing
# the old autoconf version as "autoconf" and the newer version as
# "autoconf-2.53".
#
# This program DOES NOT WORK with ash or zsh. Get a real shell. Most
# /bin/sh are fine, bash, ksh, pdksh are also fine.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
# USA
###############################################################################

isinpath() {
    (
    IFS=:
    for i in $PATH ; do
	if [ -x $i/$1 ] ; then
	    return 0;
	fi
    done
    return 1
    )
}

try() {
    opts=
    prg=$1
    shift
    while [ "$1" != "--" ] ; do
	opts="$opts $1"
	shift
    done
    shift
    l=$prg
    set -- $@
    while [ "$1" ] ; do
	if isinpath "$prg-$1" ; then
	    prg=$prg-$1
	    break
	fi
	shift
    done
    isinpath "$prg" || prg=
    if [ "x$prg" = "x" ] ; then
	echo "$l: not found" >&2
	exit 1
    fi
    (
	set -x
	$prg $opts
    )
}

# -------------------------

set -e
unset IFS

amlist='1.7 1.6.3 1.6 1.5'
aclist='2.54 2.53 2.52'

try aclocal -- $amlist
try autoheader -- $aclist
try automake -a -f -- $amlist
try autoconf -- $aclist
./configure "$@"
