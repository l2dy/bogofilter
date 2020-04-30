This describes the conversion from SVN to Git.

Earlier attempts with svn2git, git-svn or svn-all-fast-export (KDE's)[1] version were unsatisfactory,
and in Mid May 2019, Eric S. Raymond's reposurgeon (post-3.45 Git version) had
matured sufficiently that I dared the conversion with it, running on Fedora 30/x86_64 on 2019-05-19.

This requires:
- ESR's reposurgeon, Git version e5dcc35732bde280ff808f43d6815484c77c11d1 (v3.45 may work, untested)
  <http://www.catb.org/~esr/reposurgeon/>
  Git repo: <https://gitlab.com/esr/reposurgeon>
  + this was installed under /usr/local, and when I compiled it, these were installed:
    golang-1.12.5-1.fc30.x86_64 gcc-go-9.1.1-1.fc30.x86_64
- bash-5.0.7-1.fc30.x86_64
- git-2.21.0-1.fc30.x86_64
- subversion-1.12.0-1.fc30.x86_64

And in order to speed things up
- pypy-7.0.0-1.fc30.x86_64 (for Python 2.x)

All the configuration is recorded in bogofilter.lift, bogofilter.map, bogofilter.opts, and Makefile.o

run-conversion.sh is what I ran, it's a wrapper around do-conversion.sh that would log output to conversion.log

do-conversion.sh is the glue that runs make on reposurgeon's Makefile

post-conversion-cleanup.sh is for further tinkering with the repo after reposurgeon, and was adapted from earlier
svn-all-fast-export attempts.

The conversion generates bogofilter-mirror as a SVN server-side mirror copy of the SVN repo. I faked it with rsync.
bogofilter.svn is the svn dump of bogofilter-mirror as a single file.

bogofilter.fi and bogofilter.fo are the fast-export and legacy information intermediate files.

bogofilter-git is the resulting output Git repo that ultimately got uploaded.

conversion.log.xz is the xz-compressed log file of the conversion.

[1] https://github.com/svn-all-fast-export/svn2git.git
