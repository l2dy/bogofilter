# $Id$
AC_INIT(bogofilter, 0.15.7.cvs)
AC_PREREQ(2.53)
AC_CONFIG_SRCDIR([src])
AC_CANONICAL_TARGET

dnl foreign: don't insist on ChangeLog
dnl 1.6: minimum version of automake/aclocal
dnl dist-bzip2: make dist emits bogofilter-$VERSION.tar.bz2
AM_INIT_AUTOMAKE([foreign 1.6 dist-bzip2])

AM_CONFIG_HEADER(src/config.h:config.in)
AC_PROG_AWK
AC_PROG_CC(gcc xlc cc)
AC_PROG_RANLIB
AC_PATH_PROGS(PERL, perl perl5, $PATH:/usr/local/bin:/usr/bin)
AM_PROG_LEX

# Check for extension
AC_EXEEXT

dnl crutch for b0rked S/390 gcc:
if test "x$target_cpu" = "xs390" && test "x$GCC" = "xyes" ; then
    case ${CFLAGS} in
    *-O*)
	OCFLAGS="$CFLAGS"
	dnl override -Os and -O2 and higher to -O1.
	dnl this is imperfect, as you can fool it by saying -O04
	dnl however, you can as well say -O4 -O4 and it will
	dnl be -O1 and -O4 -- the last one takes precedence in gcc.
	CFLAGS="`echo \"$CFLAGS\" | sed 's/-O[[s1-9][0-9]]*/-O1/'`"
	export CFLAGS
	AC_MSG_NOTICE([GCC for s390: Overriding CFLAGS "$OCFLAGS" to "$CFLAGS".])
    esac
fi

dnl If we're using gcc, enable some warnings
dnl missing: -Wunreachable-code
AC_CACHE_CHECK(for additional compiler options, ac_cv_prog_gcc_flags, [
ac_cv_prog_gcc_flags=""
if test "$GCC" = yes
then
  echo "void dummy(void);" >configure-dummy.c
  echo "void dummy(void) {}" >>configure-dummy.c
  addopts=
  case `$CC --version` in
      *2.7*) ;;
      *) addopts="$addopts -Wpointer-arith" ;;
  esac
  for i in -Wall -W -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
      -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings \
      -Waggregate-return -Wmissing-declarations \
      -Wmissing-format-attribute -Wnested-externs \
      -ggdb -fno-common -Wchar-subscripts -Wcomment \
      -Wimplicit -Wsequence-point -Wreturn-type \
      -Wfloat-equal $addopts \
      -Wno-system-headers
      # -Wmissing-noreturn -Wconversion -pedantic -Wredundant-decls
  do
       if $CC $i $ac_cv_prog_gcc_flags -c configure-dummy.c >/dev/null 2>&1 
       then ac_cv_prog_gcc_flags="$ac_cv_prog_gcc_flags $i"
       else echo "ignoring $i"
       fi
  done
fi])
rm -f configure-dummy.c configure-dummy.o
CFLAGS="$CFLAGS $ac_cv_prog_gcc_flags"
AC_SUBST(CFLAGS)

dnl Checks for libraries.
AC_CHECK_LIB([m],  [cos])

AC_CHECK_DECLS(optreset,,,[[
#include <unistd.h>
/* Solaris */
#include <stdlib.h>
]])

dnl Coerce GLIBC 2.3 to GLIBC 2.2 compatibility.
dnl May not be complete with future extensions, but appears to work
dnl today, 2003-10-02. Matthias Andree
AC_DEFINE(__NO_CTYPE,1,[Define __NO_CTYPE to 1 to avoid GLIBC_2.3-specific ctype.h functions.])

AC_ARG_ENABLE(static,
      AC_HELP_STRING([--enable-static], [Build static versions as well.]))
if test a$enable_static = ayes ; then
    AC_MSG_CHECKING([option to link statically])
    LDEFLAGS="$LDFLAGS"
    found=0
    for i in -static ; do
	LDFLAGS="$LDEFLAGS $i"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>]],
			  [[exit (0);]])],found=1)
	if test $found -eq 1 ; then break ; fi
    done
    if test $found -eq 1 ; then
	AC_MSG_RESULT($i)
	STATICLDFLAGS=$i
	LDFLAGS="$LDEFLAGS"
    else
	AC_MSG_RESULT([[?]])
	AC_MSG_WARN([Cannot figure out how to statically link a program.])
	STATICLDFLAGS=
	enable_static=no
    fi
fi
export STATICLDFLAGS
AC_SUBST(STATICLDFLAGS)
AM_CONDITIONAL(ENABLE_STATIC, test x$enable_static = xyes)

WITH_DB_ENGINE=db
AC_ARG_WITH(database,
	    AC_HELP_STRING([--with-database=ENGINE],
	    [Choose database engine {db|tdb|qdbm} [[ENGINE=db]]]),
	    [ WITH_DB_ENGINE=$withval ]
)
case "x$WITH_DB_ENGINE" in
    xdb|xtdb|xqdbm) : ;;
    *) AC_MSG_ERROR([Invalid --with-database argument. Supported engines are db, tdb, qdbm.])
    ;;
esac

case "x$WITH_DB_ENGINE" in
    xtdb)
AC_CHECK_LIB(tdb,
             tdb_open,,
             [AC_MSG_ERROR(Cannot link to tdb library.
                           Try setting the environment variables:
                           LDFLAGS='-L/path/to/tdb/lib' CPPFLAGS='-I/path/to/tdb/include'
                           )],
)
;;
  xqdbm) 
  AC_CHECK_LIB(qdbm, dbm_open,,[
    AC_MSG_ERROR(Cannot link to qdbm library.)])
;;
    xdb)
AH_TEMPLATE([HAVE_DB_H], [Have suitable db.h header])
need_dash_r=
case $target_os in
    solaris*) need_dash_r=yes ;;
esac
WITH_DB=

dnl old test, going to be phased out
AC_ARG_WITH(db,
	    AC_HELP_STRING([--with-db=PATH], [Specify path to BerkeleyDB install directory]),
	    [
	     if test "x$withval" != "xno" ; then
		 WITH_DB=$withval
		 if test -d "$withval/lib"; then
		     if test -n "${need_dash_r}"; then
			 LDFLAGS="-R${withval}/lib ${LDFLAGS}"
		     fi
		     LDFLAGS="-L${withval}/lib ${LDFLAGS}"
		 else
		     if test -n "${need_dash_r}"; then
			 LDFLAGS="-R${withval} ${LDFLAGS}"
		     fi
		     LDFLAGS="-L${withval} ${LDFLAGS}"
		 fi
		 if test -f "$withval/include/db.h"; then
		     CPPFLAGS="-I${withval}/include ${CPPFLAGS}"
		 else
		     if test -f "$withval/db.h";then
			 CPPFLAGS="-I${withval} ${CPPFLAGS}"
		     else
			 AC_MSG_ERROR(Not a BerkeleyDB path. Use LDFLAGS and CPPFLAGS to specify the path.)
			 fi
		     fi
		 fi
		 ]
)

if test "x$WITH_DB" != "x" ; then
    DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS --with-db=$WITH_DB"
fi

AC_CHECK_DECLS(db_create,,
	       AC_MSG_ERROR([[Can not locate a suitable BerkeleyDB db.h header file.
Use --with-db=PATH to specify the path to a v3+ install directory.
You can download BerkeleyDB 4.0.x from http://www.sleepycat.com/]]),
[[#include <db.h>]]
)


AC_CHECK_DB([db db-4.1 db41 db-4.0 db4 db-3.3 db-3.2 db-3.1 db3],${STATICLDFLAGS},,
	    AC_MSG_ERROR([[Can not locate a suitable Berkeley DB library.  Use --with-db=PATH to specify the path to a v3+ install directory]]),-lpthread -lthread)
export LIBS

dnl check if run-time link works
dnl XXX FIXME: try -rpath and -R with the library path component.
AC_MSG_CHECKING(if a program can be linked against BerkeleyDB and run)
AC_RUN_IFELSE(
  AC_LANG_PROGRAM([[
		   #include <stdlib.h>
		   #include <db.h>
		   ]], [[
			    exit(0);
		   ]]),,
		   [AC_MSG_RESULT(no)
AC_MSG_ERROR([Cannot run a program linked against libdb.
Did you adjust your library search path to include your libdb?])],)
AC_MSG_RESULT(yes)

dnl check if header and library of DB library match
dnl ignore if cross compiling
AC_MSG_CHECKING(if BerkeleyDB header and library versions match)
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
		   ]]),,
		   [AC_MSG_RESULT(no)
		   AC_MSG_ERROR(db.h header file and db library version do not match.)],)
AC_MSG_RESULT(yes)
;;
esac

# Allow the user to specify a header name to use to indicate whether a given
# message is SPAM or not.
AC_ARG_ENABLE(spam-header-name, 
	AC_HELP_STRING([--enable-spam-header-name=name], 
	[Use specified header name instead of X-Bogosity]), 
	AC_DEFINE_UNQUOTED(SPAM_HEADER_NAME, 
	["$enableval"], 
	[Use specified header name instead of X-Bogosity]),
	AC_DEFINE_UNQUOTED(SPAM_HEADER_NAME, 
	["X-Bogosity"], 
	[Use specified header name instead of X-Bogosity])
)

# Allow the user to enable memory usage debugging methods

AC_ARG_ENABLE(memdebug,
	AC_HELP_STRING([--enable-memdebug],
	[Enable memory usage debugging]),
	if test "$enableval" = no; then
		[USE_MEMDEBUG=NO]
	else
		[USE_MEMDEBUG=YES]
	fi
)

# Allow the user to disable Graham methods

AC_ARG_ENABLE(graham-method,
	AC_HELP_STRING([--disable-graham-method],
	[Disable Graham calculation method]),
	if test "$enableval" = no; then
		[USE_GRAHAM=NO]
	else
		[USE_GRAHAM=YES]
	fi
)

# Allow the user to disable Robinson methods

if test x$USE_ROBINSON != xYES ; then
AC_ARG_ENABLE(robinson-method,
	AC_HELP_STRING([--disable-robinson-method],
	[Disable Robinson calculation method]),
	if test "$enableval" = no; then
		[USE_ROBINSON=NO]
	else
		[USE_ROBINSON=YES]
	fi
)
fi

# Allow the user to disable Robinson-Fisher methods

AC_ARG_ENABLE(robinson-fisher,
	AC_HELP_STRING([--disable-robinson-fisher],
	[Disable Fisher's method of combining probabilities]),
	if test "$enableval" = no; then
		[USE_FISHER=NO]
	else
		[USE_FISHER=YES]
 		AC_DEFINE(ENABLE_ROBINSON_FISHER,1,
		[Enable Fisher's method of combining probabilities])
	fi
)

if test x$USE_GRAHAM != xNO ; then
	AC_DEFINE(ENABLE_GRAHAM_METHOD,1,
	[Enable Graham calculation method])
fi

if test x$USE_ROBINSON != xNO ; then
	AC_DEFINE(ENABLE_ROBINSON_METHOD,1,
	[Enable Robinson calculation method])
fi

if test x$USE_FISHER != xNO ; then
	AC_DEFINE(ENABLE_ROBINSON_FISHER,1,
	[Enable Fisher's method of combining P])
	[USE_ROBINSON=YES]
fi

# Enable building with Graham/Robinson/Fisher files.
AM_CONDITIONAL(ENABLE_GRAHAM_METHOD, test x$USE_GRAHAM != xNO)
AM_CONDITIONAL(ENABLE_ROBINSON_METHOD, test x$USE_ROBINSON != xNO)
AM_CONDITIONAL(ENABLE_ROBINSON_FISHER, test x$USE_FISHER != xNO)

# Enable building with memory usage debugging files.

if test x$USE_MEMDEBUG = xYES ; then
	AC_DEFINE(ENABLE_MEMDEBUG,1,
	[Enable memory usage debugging])
fi

AM_CONDITIONAL(ENABLE_MEMDEBUG, test x$MEM_DEBUG != xNO)

case "x$WITH_DB_ENGINE" in
    xtdb)
    AC_DEFINE(ENABLE_TDB_DATASTORE,1, [Enable tdb datastore])
    AC_DEFINE_UNQUOTED(DB_TYPE, "tdb", [database type])
    AC_DEFINE_UNQUOTED(DB_EXT, ".tdb", [database file extension])
    ;;
    xqdbm)
    AC_DEFINE(ENABLE_QDBM_DATASTORE,1, [Enable qdbm datastore])
    AC_DEFINE_UNQUOTED(DB_TYPE, "qdbm")
    AC_DEFINE_UNQUOTED(DB_EXT, ".qdbm")
    ;;
    xdb)
    AC_DEFINE_UNQUOTED(DB_TYPE, "db")
    AC_DEFINE_UNQUOTED(DB_EXT, ".db")
    ;;
esac

AM_CONDITIONAL(ENABLE_TDB_DATASTORE, test "x$WITH_DB_ENGINE" = "xtdb")
AM_CONDITIONAL(ENABLE_QDBM_DATASTORE, test "x$WITH_DB_ENGINE" = "xqdbm")

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_CHECK_HEADERS([syslog.h sys/param.h fcntl.h stdlib.h string.h sys/socket.h unistd.h sys/file.h stddef.h sys/time.h sys/select.h inttypes.h stdint.h])
AC_CHECK_HEADERS([limits.h float.h],,[AC_CHECK_HEADERS(values.h)])

AC_C_INLINE
if test "$ac_cv_c_inline" != no ; then
    AC_DEFINE(HAVE_INLINE,1,[Define to 1 if your compiler supports inline])
    AC_SUBST(HAVE_INLINE)
fi


# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_SIGNAL
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_TYPES([ulong, uint32_t, u_int32_t, int32_t, int16_t, u_int16_t, uint16_t, u_int8_t])
AC_CACHE_SAVE
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
dnl The next one isn't really needed, but useful for post-mortem analysis of
dnl automated package build logs such as bento.freebsd.org or buildd.debian.org:
AC_CHECK_SIZEOF(size_t)

# Checks for library functions.
dnl AC_FUNC_MALLOC
# AC_FUNC_MALLOC wants rpl_malloc in malloc.c, to protect against
# malloc(0) == NULL. xmalloc handles this by retrying with size 1.

dnl AC_FUNC_STAT
# AC_FUNC_STAT wants to replace stat(2) if stat("", &st) succeeds.
# we catch this.

AC_FUNC_MEMCMP
AC_FUNC_MMAP
AC_FUNC_VPRINTF

AC_CHECK_FUNCS(alarm bzero ftruncate memset mkdir socket strdup strerror flock lockf fcntl strchr strrchr memcpy munmap pow sqrt strcasecmp strcspn strspn strstr snprintf vsnprintf)
AC_REPLACE_FUNCS(strlcpy strlcat)

dnl Use TRIO to replace missing snprintf/vsnprintf.
needtrio=0
if test "x$ac_cv_func_snprintf" != "xyes" ; then
    AC_DEFINE(snprintf, trio_snprintf, [Define to trio_snprintf if your system lacks snprintf])
    needtrio=1
fi
if test "x$ac_cv_func_vsnprintf" != "xyes" ; then
    AC_DEFINE(vsnprintf, trio_vsnprintf, [Define to trio_vsnprintf if your system lacks vsnprintf])
    needtrio=1
fi
if test $needtrio -gt 0
then
dnl BEGIN TRIO
dnl Alpha floating-point compiler option.
dnl

AC_MSG_CHECKING(for IEEE compilation options)
AC_CACHE_VAL(ac_cv_ieee_option, [
AC_TRY_COMPILE(,[
#if !(defined(__alpha) && (defined(__DECC) || defined(__DECCXX) || (defined(__osf__) && defined(__LANGUAGE_C__))) && (defined(VMS) || defined(__VMS)))
# error "Option needed"
#endif
],ac_cv_ieee_option="/IEEE_MODE=UNDERFLOW_TO_ZERO/FLOAT=IEEE",
AC_TRY_COMPILE(,[
#if !(defined(__alpha) && (defined(__DECC) || defined(__DECCXX) || (defined(__osf__) && defined(__LANGUAGE_C__))) && !(defined(VMS) || defined(__VMS)) && !defined(_CFE))
# error "Option needed"
#endif
],ac_cv_ieee_option="-ieee",
AC_TRY_COMPILE(,[
#if !(defined(__alpha) && (defined(__GNUC__) && (defined(__osf__) || defined(__linux__))))
# error "Option needed"
#endif
],ac_cv_ieee_option="-mieee",
ac_cv_ieee_option="none"
)
)
)
])
AC_MSG_RESULT($ac_cv_ieee_option)
if test $ac_cv_ieee_option != none; then
  CFLAGS="${CFLAGS} ${ac_cv_ieee_option}"
fi
dnl END TRIO

    LIBS="$LIBS -ltrio -lm"
    AC_DEFINE(NEEDTRIO, 1, [Define to 1 if trio is to be included.])
fi
AM_CONDITIONAL(NEEDTRIO, test "$needtrio" = "1")

dnl check if XMLTO is somewhere
AC_CHECK_PROG(ac_cv_enable_xmlto, xmlto, yes, no)
AC_MSG_CHECKING([whether xmlto is available])
AC_MSG_RESULT($ac_cv_enable_xmlto)

AM_CONDITIONAL(ENABLE_XMLTO, test x$ac_cv_enable_xmlto = xyes)
dnl end of tests

dnl BEGIN EMBEDDED GSL
AC_CACHE_CHECK([for extended floating point registers],ac_cv_c_extended_fp,
	       [case "$host" in
		*sparc*-*-*) ac_cv_c_extended_fp=no ;;     
		*powerpc*-*-*) ac_cv_c_extended_fp=no ;;      
		*hppa*-*-*) ac_cv_c_extended_fp=no ;;      
		*alpha*-*-*) ac_cv_c_extended_fp=no ;;      
		*68k*-*-*) ac_cv_c_extended_fp=yes ;;      
		*86*-*-*) ac_cv_c_extended_fp=yes ;;      
		*) ac_cv_c_extended_fp=unknown ;;
	    esac])

if test $ac_cv_c_extended_fp != "no" ; then
  AC_DEFINE(HAVE_EXTENDED_PRECISION_REGISTERS,1)
  AC_SUBST(HAVE_EXTENDED_PRECISION_REGISTERS)
fi
dnl END EMBEDDED GSL

AH_BOTTOM([
#if HAVE_EXTENDED_PRECISION_REGISTERS
#define GSL_COERCE_DBL(x) (gsl_coerce_double(x))
#else
#define GSL_COERCE_DBL(x) (x)
#endif
])


AC_CACHE_SAVE

if test "$sysconfdir" = "\${prefix}/etc" \
    && { test "$prefix" = "NONE" || test "$prefix" = "/usr" \
      || test "$prefix" = "/usr/" ; } ; then
    if test "$( (uname -s) )" = "Linux" ; then
      sysconfdir=/etc
    fi
    (
    prefix=$ac_default_prefix
    sysconfdir=`eval echo "$sysconfdir"`
    AC_MSG_NOTICE([configuration directory is "$sysconfdir".])
    AC_MSG_NOTICE([use --sysconfdir to override.])
    )
fi

dnl work around Linux ldconfig design bug
dnl ldconfig will cache libraries in order found in library search
dnl order rather than by minor version, so it will happily link against
dnl /usr/lib/libgsl.so.0 (which is 0.4.0) when
dnl /usr/local/lib/libgsl.so.0 (which is 0.5.0) is available
dnl so try to find gsl-config in trusted system path first

AC_MSG_NOTICE([-------------------------------------------------------------])
AC_MSG_NOTICE([If you have multiple versions of GSL installed, this])
AC_MSG_NOTICE([configure script will prefer the one in /lib or /usr/lib])
AC_MSG_NOTICE([to work around a bug in Linux ldconfig.])
AC_MSG_NOTICE([Deinstall the older GSL version if this behaviour annoys you.])
AC_MSG_NOTICE([-------------------------------------------------------------])

PATH_SAVED="$PATH"
PATH=/bin:/usr/bin:$PATH

need_my_gsl=1
AM_PATH_GSL(1.4, AC_DEFINE(HAVE_GSL_14,,[Define if you have GNU Scientific Library 1.4 or newer])
need_my_gsl=0)
AM_PATH_GSL(1.0, AC_DEFINE(HAVE_GSL_10,,[Define if you have GNU Scientific Library 1.0 or newer])
need_my_gsl=0)
if test "$need_my_gsl" = 1 ; then
  GSL_LIBS="libbf_gsl.a"
fi

PATH="$PATH_SAVED"

AM_CONDITIONAL(NEED_GSL,test $need_my_gsl = 1)

have_dosish_system=no
case "${target}" in
    *-*-mingw32*)
        # special stuff for Windoze NT
        have_dosish_system=yes
        ;;
    i?86-emx-os2 | i?86-*-os2*emx )
        # OS/2 with the EMX environment
        have_dosish_system=yes
        ;;
    i?86-*-msdosdjgpp*)
        # DOS with the DJGPP environment
        have_dosish_system=yes
        ;;
    *)
       ;;
esac

# Note the \\\\ for backslashes.  Autoconf eats one layer, leaving \\

AC_DEFINE(CURDIR_S, ".", [Define name of current directory (C string)])
AC_DEFINE(EXTSEP_S, ".", [Define file extension separator (C string)])

if test "$have_dosish_system" = no; then
AC_DEFINE(DIRSEP_C, '/', [Define directory separator (C character)])
AC_DEFINE(DIRSEP_S, "/", [Define directory separator (C string)])
else
AC_DEFINE(DIRSEP_C, '\\\\')
AC_DEFINE(DIRSEP_S, "\\\\")
fi

AC_SUBST(DISTCHECK_CONFIGURE_FLAGS)
AC_CONFIG_FILES([Makefile \
		bogofilter.spec \
		trio/Makefile \
		contrib/Makefile doc/Makefile \
		src/Makefile \
		src/tests/Makefile \
		tuning/Makefile \
		])
AC_OUTPUT
