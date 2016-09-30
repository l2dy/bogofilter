#! /bin/sh

# Source SVN repository in server-side format (svnsync, rsync, ...)
srcrepo="$HOME/VCS-mine/bogofilter.svnroot/"

# User map (login = First Last <em@il.example.org>)
usermap="$HOME/VCS-mine/bogofilter.svn/users.txt"

# Rules (shouldn't need to change these)
rules="$(pwd)"/bogofilter-svn2git.rules

set -xe
cd $(dirname "$0")
export LC_ALL=C
exec ./svn-all-fast-export \
    --svn-branches --empty-dirs --add-metadata --stats \
    --rules "${rules}" \
    --identity-map "${usermap}" \
    "${srcdir}"
