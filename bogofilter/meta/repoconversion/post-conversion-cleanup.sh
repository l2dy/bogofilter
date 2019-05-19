#! /bin/bash
# requires a shell with input redirection of a "cmd1 <(cmd2)" form.

set -eu
cd bogofilter-git

# -------------------------------------------------------

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

echo
echo "PASS 1 - convert short branches to tags"
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
	echo -e "BRANCH TO TAG\t$i"
	git show -s --format=oneline $oa | sed 's/^/  /'
	git tag -f "$i" -m "This is a tag that marks a shallow branch '$i' before the SVN-to-Git conversion" "$i" # XXX TODO - create tags as commits instead?
	git branch -D "$i"
	echo
    else
	echo -e "BRANCH KEPT\t$i"
	# git log --format=oneline $oa..$bp | sed 's/^/  /'
	echo
    fi
done

##################################
echo
echo "PASS 2 - rename old branches"
for i in $(git branch -q --format='%(refname:short)') ; do
	case $i in
		*old*|*merged*|*master|*avoid-rfc2047*|*lmdb*)
			continue ;;
		*unlabeled*)
			( set -x ; git branch -m "$i" "unlabeled_cvs_branches/$i" ) ;;
		*)
			( set -x ; git branch -m "$i" "old_branches/$i" ) ;;
	esac
done

##################################
set -x
echo
echo "PASS 3 - salvage dangling objects"
env LC_ALL=C LANG=en git -c core.commitGraph=true fsck --full --strict |\
	  grep ^dangling |\
	  while read nil typ id ; do
		  git tag dangling-$typ-$id $id
	  done

echo
echo "STEP 4 - tag result"
git tag -a svn-to-git-conversion -m "This tag marks the conversion from SVN to Git."

echo
echo "STEP 5 - tag result and compress"
git reflog expire --expire=now --all
git pack-refs --all
git -c gc.aggressiveWindow=1250 -c gc.aggressiveDepth=250 gc --aggressive
