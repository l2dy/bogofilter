#! /bin/bash

###############################################################################
# cvs-bootstrap.sh -- rebuild auto* files after fresh CVS checkout
# (C) 2002 by Matthias Andree
#
# this program tries to work around Red Hat braindeadness installing
# "autoconf" as the old version and the newer version under
# "autoconf-2.53"
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

try() {
    opts=
    prg=
    while [ "$1" != "--" ] ; do
	opts="$opts $1"
	shift
    done
    shift
    while [ "$1" ] ; do
	type -p $1 >/dev/null && { prg=$1 ; break ; }
	l=$1
	shift
    done
    if test "$prg" = "" ; then
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

am='{-1.6.3,-1.6,-1.5,}'
ac='{-2.54,-2.53,-2.52,}'
eval try    -- aclocal$am
eval try    -- autoheader$ac
eval try -a -- automake$am
eval try    -- autoconf$ac
./configure "$@"
