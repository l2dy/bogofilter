dnl -*- Autoconf -*-
dnl The AC_HEADER_STDBOOL code was taken from autoconf 2.57.
dnl
dnl It was modified to only define the AC_HEADER_STDBOOL test,
dnl which is not present in autoconf 2.54 and older, because
dnl many distributions (Red Hat Linux 8.0, SuSE Linux 8.1) still ship
dnl autoconf 2.53.
dnl -- Matthias Andree

# This file is part of Autoconf.                       -*- Autoconf -*-
# Checking for headers.
#
# Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# As a special exception, the Free Software Foundation gives unlimited
# permission to copy, distribute and modify the configure scripts that
# are the output of Autoconf.  You need not follow the terms of the GNU
# General Public License when using or distributing such scripts, even
# though portions of the text of Autoconf appear in them.  The GNU
# General Public License (GPL) does govern all other use of the material
# that constitutes the Autoconf program.
#
# Certain portions of the Autoconf source text are designed to be copied
# (in certain cases, depending on the input) into the output of
# Autoconf.  We call these the "data" portions.  The rest of the Autoconf
# source text consists of comments plus executable code that decides which
# of the data portions to output in any given case.  We call these
# comments and executable code the "non-data" portions.  Autoconf never
# copies any of the non-data portions into its output.
#
# This special exception to the GPL applies to versions of Autoconf
# released by the Free Software Foundation.  When you make and
# distribute a modified version of Autoconf, you may extend this special
# exception to the GPL to apply to your modified version as well, *unless*
# your modified version has the potential to copy into its output some
# of the text that was the non-data portion of the version that you started
# with.  (In other words, unless your change moves or copies text from
# the non-data portions to the data portions.)  If your modification has
# such potential, you must delete any notice of this special exception
# to the GPL from your modified version.
#
# Written by David MacKenzie, with help from
# Fran�ois Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
# Roland McGrath, Noah Friedman, david d zuhn, and many others.

AC_DEFUN([AC_HEADER_STDBOOL],
[AC_CACHE_CHECK([for stdbool.h that conforms to C99],
   [ac_cv_header_stdbool_h],
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
      [[
#include <stdbool.h>
#ifndef bool
# error bool is not defined
#endif
#ifndef false
# error false is not defined
#endif
#if false
# error false is not 0
#endif
#ifndef true
# error true is not defined
#endif
#if true != 1
# error true is not 1
#endif
#ifndef __bool_true_false_are_defined
# error __bool_true_false_are_defined is not defined
#endif

        struct s { _Bool s: 1; _Bool t; } s;

        char a[true == 1 ? 1 : -1];
        char b[false == 0 ? 1 : -1];
        char c[__bool_true_false_are_defined == 1 ? 1 : -1];
        char d[(bool) -0.5 == true ? 1 : -1];
        bool e = &s;
        char f[(_Bool) -0.0 == false ? 1 : -1];
        char g[true];
        char h[sizeof (_Bool)];
        char i[sizeof s.t];
      ]],
      [[ return !a + !b + !c + !d + !e + !f + !g + !h + !i; ]])],
      [ac_cv_header_stdbool_h=yes],
      [ac_cv_header_stdbool_h=no])])
AC_CHECK_TYPES([_Bool])
if test $ac_cv_header_stdbool_h = yes; then
  AC_DEFINE(HAVE_STDBOOL_H, 1, [Define to 1 if stdbool.h conforms to C99.])
fi
])# AC_HEADER_STDBOOL

dnl This is the end of the part extracted from autoconf.
dnl The next part was added by Clint Adams and modified by
dnl Matthias Andree.

dnl arguments:
dnl 1- space delimited list of libraries to check for db_create
dnl 2- optional LDFLAGS to apply when checking for library, such as -static
dnl 3- action-if-found
dnl 4- action-if-not-found
dnl 5- optional set of libraries to use (pass -lpthread here
dnl    in case DB is compiled with POSIX mutexes)
AC_DEFUN([AC_CHECK_DB],[
  AS_VAR_PUSHDEF([ac_tr_db], [ac_cv_db_libdb])dnl
  bogo_saved_LIBS="$LIBS"
  bogo_saved_LDFLAGS="$LDFLAGS"
  AC_CACHE_CHECK([for library providing db_create], ac_tr_db, [
    for lib in '' $1 ; do
     for i in '' $5 ; do
      for ld in '' $2 ; do
	if test "x$lib" != "x" ; then
	  bogo_libadd="-l$lib $i"
	else
	  bogo_libadd="$i"
	fi
	LDFLAGS="$bogo_saved_LDFLAGS $ld"
	LIBS="$LIBS $bogo_libadd"
	AC_RUN_IFELSE(
	    AC_LANG_PROGRAM([[
		   #include <stdlib.h>
		   #include <db.h>
		   ]], [[
			int maj, min;
			(void)db_version(&maj, &min, (void *)0);
			if (maj != DB_VERSION_MAJOR) exit(1);
			  if (min != DB_VERSION_MINOR) exit(1);
			    exit(0);
		   ]]),
	    [AS_VAR_SET(ac_tr_db, $bogo_libadd)],
	    [AS_VAR_SET(ac_tr_db, no)],
	    AC_LINK_IFELSE([AC_LANG_PROGRAM([
		#include <db.h>],[
		int foo=db_create((void *)0, (void *) 0, 0 );
	    ])],
	    [AS_VAR_SET(ac_tr_db, $bogo_libadd)],
	    [AS_VAR_SET(ac_tr_db, no)]))

	AS_IF([test x"AS_VAR_GET(ac_tr_db)" != xno],
	    [$3
	    db="$bogo_libadd"],
	    [LIBS="$bogo_saved_LIBS"
	    db=no])
	LDFLAGS="$bogo_saved_LDFLAGS"
	test "x$db" = "xno" && break
      done
      test "x$db" != "xno" && break
     done
     test "x$db" != "xno" && break
    done
    ])
if test "x$db" = "xno"; then
$4
else
    LIBS="$bogo_saved_LIBS $ac_tr_db"
fi
AS_VAR_POPDEF([ac_tr_db])dnl
])# AC_CHECK_DB

