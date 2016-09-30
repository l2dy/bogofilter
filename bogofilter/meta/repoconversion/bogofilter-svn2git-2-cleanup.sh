#! /bin/bash
# requires a shell with input redirection of a "cmd1 <(cmd2)" form.

set -eu

oldest_ancestor() {
    local masterrevs=
    
    if [ -z "${masterrevs}" ] ; then
	masterrevs=$(git rev-list --first-parent master)
	if [ -z "${masterrevs}" ] ; then
	    echo >&2 "can't get master's rev-list. Aborting."
	    exit 1
	fi
    fi

    printf "%s\n" "${masterrevs}" | \
    diff --old-line-format= --new-line-format= \
	- \
	<(git rev-list --first-parent "${2:-HEAD}") \
    | head -1
}

# -------------------------------------------------------

# 1. nuke unlabeled branches
for i in $(git branch --list unlabeled*) ; do
    git branch -D "$i"
done

# 2. get the easy stuff out of the way, convert 1-deep branches to tags
branches=$(git branch --list | grep -v '\*')
for i in $branches ; do
    # get the commit BEFORE a commit on a new branch:
    oa=$(oldest_ancestor master $i)
    # get the parent of the branch's tip
    bp=$(git rev-list --max-count=1 ${i}^) || \
	{ echo >&2 "WARN: can't find parent of ${i}" ; continue ; }
    # if they are the same, the branch is only one commit deep,
    # and that is the commit that created the branch (actually a copy)
    # in the cvs2svn conversion, so report as a TAG.
    if [ -z "$oa" -o -z "$bp" ] ; then
	echo >&2 "WARN: oa=$oa, bp=$bp"
    fi

    if [ $oa = $bp ] ; then
	echo -e "TAG\t$i"
	git show -s --format=oneline $oa | sed 's/^/  /'
	git tag "$i" "$i" # XXX TODO - create tags as commits instead?
	git branch -D "$i"
	echo
    else
	echo -e "BRNCH\t$i"
	git log --format=oneline $oa..$bp | sed 's/^/  /'
	echo
    fi
done

# 3. deep scan, figure out which "branches" are only 1-deep into another
# branch, these can also become tags:
branches="$(git branch |sed 's/\*//')"
for i in $branches ; do
    revs=$(git rev-list $i --not $(echo "$branches" | grep -v "$i"))
    echo "$i:" ; echo "$revs"|sed 's/^/  /'
done
