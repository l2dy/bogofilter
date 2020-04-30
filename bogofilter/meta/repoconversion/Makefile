# Makefile for bogofilter conversion using reposurgeon
#
# Steps to using this:
# 1. Make sure reposurgeon and repotool are on your $PATH.
#    For large repositories It is usually best to run reposurgeon using
#    PyPy: set REPOSURGEON to "pypy" followed by an absolute pathname
#    to reposurgeon.
# 2. (Skip this step if you're starting from a stream file.) For svn, set
#    REMOTE_URL to point at the remote repository you want to convert.
#    If the repository is already in a DVCS such as hg or git,
#    set REMOTE_URL to either the normal cloning URL (starting with hg://,
#    git://, etc.) or to the path of a local clone.
# 3. For cvs, set CVS_HOST to the repo hostname and CVS_MODULE to the module,
#    then uncomment the line that builds REMOTE_URL 
#    Note: for CVS hosts other than Sourceforge or Savannah you will need to 
#    include the path to the CVS modules directory after the hostname.
# 4. Set any required read options, such as --user-ignores or --nobranch,
#    by setting READ_OPTIONS.
# 5. Run 'make stubmap' to create a stub author map.
# 6. (Optional) set REPOSURGEON to point at a faster cython build of the tool.
# 7. Run 'make' to build a converted repository.
#
# The reason both first- and second-stage stream files are generated is that,
# especially with Subversion, making the first-stage stream file is often
# painfully slow. By splitting the process, we lower the overhead of
# experiments with the lift script.
#
# For a production-quality conversion you will need to edit the map
# file and the lift script.  During the process you can set EXTRAS to
# name extra metadata such as a comments message-box.
#
# Afterwards, you can use the headcompare and tagscompare productions
# to check your work.
#

EXTRAS = 
REMOTE_URL = svn://svn.debian.org/bogofilter
#REMOTE_URL = https://bogofilter.googlecode.com/svn/
REMOTE_URL = file:///home/mandree/VCS-mine/bogofilter.svnroot/
CVS_HOST = bogofilter.cvs.sourceforge.net
#CVS_HOST = cvs.savannah.gnu.org
CVS_MODULE = bogofilter
#REMOTE_URL = cvs://$(CVS_HOST)/bogofilter\#$(CVS_MODULE)
READ_OPTIONS = --preserve --use-uuid
VERBOSITY = "verbose 1"
# pypy2 reposurgeon is A LOT faster than pypy3.
REPOSURGEON = pypy2 /usr/local/bin/reposurgeon

# Configuration ends here

.PHONY: local-clobber remote-clobber gitk gc compare clean dist stubmap
# Tell make not to auto-remove tag directories, because it only tries rm 
# and hence fails
.PRECIOUS: bogofilter-%-checkout bogofilter-%-git

default: bogofilter-git

# Build the converted repo from the second-stage fast-import stream
bogofilter-git: bogofilter.fi
	rm -fr bogofilter-git; $(REPOSURGEON) 'read <bogofilter.fi' 'prefer git' 'rebuild bogofilter-git'

# Build the second-stage fast-import stream from the first-stage stream dump
bogofilter.fi: bogofilter.svn bogofilter.opts bogofilter.lift bogofilter.map $(EXTRAS)
	$(REPOSURGEON) $(VERBOSITY) 'script bogofilter.opts' "read $(READ_OPTIONS) <bogofilter.svn" 'authors read <bogofilter.map' 'sourcetype svn' 'prefer git' 'script bogofilter.lift' 'legacy write >bogofilter.fo' 'write >bogofilter.fi'

# Build the first-stage stream dump from the local mirror
bogofilter.svn: bogofilter-mirror
	(cd bogofilter-mirror/ >/dev/null; repotool export) >bogofilter.svn

# Build a local mirror of the remote repository
bogofilter-mirror:
	repotool mirror $(REMOTE_URL) bogofilter-mirror

# Make a local checkout of the source mirror for inspection
bogofilter-checkout: bogofilter-mirror
	cd bogofilter-mirror >/dev/null; repotool checkout $(PWD)/bogofilter-checkout

# Make a local checkout of the source mirror for inspection at a specific tag
bogofilter-%-checkout: bogofilter-mirror
	cd bogofilter-mirror >/dev/null; repotool checkout $(PWD)/bogofilter-$*-checkout $*

# Force rebuild of first-stage stream from the local mirror on the next make
local-clobber: clean
	rm -fr bogofilter.fi bogofilter-git *~ .rs* bogofilter-conversion.tar.gz bogofilter-*-git

# Force full rebuild from the remote repo on the next make.
remote-clobber: local-clobber
	rm -fr bogofilter.svn bogofilter-mirror bogofilter-checkout bogofilter-*-checkout

# Get the (empty) state of the author mapping from the first-stage stream
stubmap: bogofilter.svn
	$(REPOSURGEON) "read $(READ_OPTIONS) <bogofilter.svn" 'authors write >bogofilter.map'

# Compare the histories of the unconverted and converted repositories at head
# and all tags.
EXCLUDE = -x CVS -x .svn -x .git
EXCLUDE += -x .svnignore -x .gitignore
headcompare: bogofilter-mirror bogofilter-git
	repotool compare $(EXCLUDE) bogofilter-mirror bogofilter-git
tagscompare: bogofilter-mirror bogofilter-git
	repotool compare-tags $(EXCLUDE) bogofilter-mirror bogofilter-git
branchescompare: bogofilter-mirror bogofilter-git
	repotool compare-branches $(EXCLUDE) bogofilter-mirror bogofilter-git
allcompare: bogofilter-mirror bogofilter-git
	repotool compare-all $(EXCLUDE) bogofilter-mirror bogofilter-git

# General cleanup and utility
clean:
	rm -fr *~ .rs* bogofilter-conversion.tar.gz *.svn *.fi *.fo

# Bundle up the conversion metadata for shipping
SOURCES = Makefile bogofilter.lift bogofilter.map $(EXTRAS)
bogofilter-conversion.tar.gz: $(SOURCES)
	tar --dereference --transform 's:^:bogofilter-conversion/:' -czvf bogofilter-conversion.tar.gz $(SOURCES)

dist: bogofilter-conversion.tar.gz

#
# The following productions are git-specific
#

# Browse the generated git repository
gitk: bogofilter-git
	cd bogofilter-git; gitk --all

# Run a garbage-collect on the generated git repository.  Import doesn't.
# This repack call is the active part of gc --aggressive.  This call is
# tuned for very large repositories.
gc: bogofilter-git
	cd bogofilter-git; time git -c pack.threads=1 repack -AdF --window=1250 --depth=250
