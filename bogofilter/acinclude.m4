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
# François Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
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

# Configure path for the GNU Scientific Library
# Christopher R. Gabriel <cgabriel@linux.it>, April 2000


AC_DEFUN(AM_PATH_GSL,
[
AC_ARG_WITH(gsl-prefix,[  --with-gsl-prefix=PFX   Prefix where GSL is installed (optional)],
            gsl_prefix="$withval", gsl_prefix="")
AC_ARG_WITH(gsl-exec-prefix,[  --with-gsl-exec-prefix=PFX Exec prefix where GSL is installed (optional)],
            gsl_exec_prefix="$withval", gsl_exec_prefix="")
AC_ARG_ENABLE(gsltest, [  --disable-gsltest       Do not try to compile and run a test GSL program],
		    , enable_gsltest=yes)

  if test "x${GSL_CONFIG+set}" != xset ; then
     if test "x$gsl_prefix" != x ; then
         GSL_CONFIG="$gsl_prefix/bin/gsl-config"
     fi
     if test "x$gsl_exec_prefix" != x ; then
        GSL_CONFIG="$gsl_exec_prefix/bin/gsl-config"
     fi
  fi

  AC_PATH_PROG(GSL_CONFIG, gsl-config, no)
  min_gsl_version=ifelse([$1], ,0.2.5,$1)
  AC_MSG_CHECKING(for GSL - version >= $min_gsl_version)
  no_gsl=""
  if test "$GSL_CONFIG" = "no" ; then
    no_gsl=yes
  else
    GSL_CFLAGS=`$GSL_CONFIG --cflags`
    GSL_LIBS=`$GSL_CONFIG --libs`

    gsl_major_version=`$GSL_CONFIG --version | \
           sed 's/^\([[0-9]]*\).*/\1/'`
    if test "x${gsl_major_version}" = "x" ; then
       gsl_major_version=0
    fi

    gsl_minor_version=`$GSL_CONFIG --version | \
           sed 's/^\([[0-9]]*\)\.\{0,1\}\([[0-9]]*\).*/\2/'`
    if test "x${gsl_minor_version}" = "x" ; then
       gsl_minor_version=0
    fi

    gsl_micro_version=`$GSL_CONFIG --version | \
           sed 's/^\([[0-9]]*\)\.\{0,1\}\([[0-9]]*\)\.\{0,1\}\([[0-9]]*\).*/\3/'`
    if test "x${gsl_micro_version}" = "x" ; then
       gsl_micro_version=0
    fi

    if test "x$enable_gsltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GSL_CFLAGS"
      LIBS="$LIBS $GSL_LIBS"

      rm -f conf.gsltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* my_strdup (const char *str);

char*
my_strdup (const char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = (char *)malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main (void)
{
  int major = 0, minor = 0, micro = 0;
  int n;
  char *tmp_version;

  system ("touch conf.gsltest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_gsl_version");

  n = sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) ;

  if (n != 2 && n != 3) {
     printf("%s, bad version string\n", "$min_gsl_version");
     exit(1);
   }

   if (($gsl_major_version > major) ||
      (($gsl_major_version == major) && ($gsl_minor_version > minor)) ||
      (($gsl_major_version == major) && ($gsl_minor_version == minor) && ($gsl_micro_version >= micro)))
    {
      exit(0);
    }
  else
    {
      printf("\n*** 'gsl-config --version' returned %d.%d.%d, but the minimum version\n", $gsl_major_version, $gsl_minor_version, $gsl_micro_version);
      printf("*** of GSL required is %d.%d.%d. If gsl-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If gsl-config was wrong, set the environment variable GSL_CONFIG\n");
      printf("*** to point to the correct copy of gsl-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      exit(1);
    }
}

],, no_gsl=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gsl" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GSL_CONFIG" = "no" ; then
       echo "*** The gsl-config script installed by GSL could not be found"
       echo "*** If GSL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GSL_CONFIG environment variable to the"
       echo "*** full path to gsl-config."
     else
       if test -f conf.gsltest ; then
        :
       else
          echo "*** Could not run GSL test program, checking why..."
          CFLAGS="$CFLAGS $GSL_CFLAGS"
          LIBS="$LIBS $GSL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GSL or finding the wrong"
          echo "*** version of GSL. If it is not finding GSL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GSL was incorrectly installed"
          echo "*** or that you have moved GSL since it was installed. In the latter case, you"
          echo "*** may want to edit the gsl-config script: $GSL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
#     GSL_CFLAGS=""
#     GSL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GSL_CFLAGS)
  AC_SUBST(GSL_LIBS)
  rm -f conf.gsltest
])


