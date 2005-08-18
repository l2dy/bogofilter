/* config.  RISC OS version, hand-crafted by Stefan Bellon.  */

/* Enable Russian language support */
/* #undef CP866 */

/* Define name of current directory (C string) */
#define CURDIR_S "@"

/* database file extension */
#define DB_EXT "/db"

/* database type */
#define DB_TYPE "db"

/* Use specified charset instead of us-ascii */
#define DEFAULT_CHARSET "iso-8859-1"

/* Define directory separator (C character) */
#define DIRSEP_C '.'

/* Define directory separator (C string) */
#define DIRSEP_S "."

/* Disable transactions in Berkeley DB */
/* #undef DISABLE_TRANSACTIONS */

/* Enable Berkeley DB datastore */
/* #undef ENABLE_DB_DATASTORE */

/* Enable iconv for converting character sets */
/* #define ENABLE_ICONV 1 */

/* Enable memory usage debugging */
/* #define ENABLE_MEMDEBUG 1 */

/* Enable qdbm datastore */
/* #define ENABLE_QDBM_DATASTORE 1 */

/* Enable sqlite datastore */
/* #define ENABLE_SQLITE_DATASTORE 1 */

/* Enable tdb datastore */
/* #define ENABLE_TDB_DATASTORE 1 */

/* Enable transactions in Berkeley DB */
/* #undef ENABLE_TRANSACTIONS */

/* Define file extension separator (C string) */
#define EXTSEP_S "/"

/* Have suitable db.h header */
/* #undef HAVE_DB_H */

/* Define to 1 if you have the declaration of `db_create', and to 0 if you
   don't. */
/* #undef HAVE_DECL_DB_CREATE */

/* Define to 1 if you have the declaration of `getopt', and to 0 if you don't.
   */
#define HAVE_DECL_GETOPT 1

/* Define to 1 if you have the declaration of `optreset', and to 0 if you
   don't. */
/* #undef HAVE_DECL_OPTRESET */

/* Define to 1 if you have the declaration of `O_DSYNC', and to 0 if you
   don't. */
/* #undef HAVE_DECL_O_DSYNC */

/* Define to 1 if you have the declaration of `O_FSYNC', and to 0 if you
   don't. */
#define HAVE_DECL_O_FSYNC 1

/* Define to 1 if you have the declaration of `O_SYNC', and to 0 if you don't.
   */
#define HAVE_DECL_O_SYNC 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if your compiler supports extern inline storage class. */
/* #undef HAVE_EXTERN_INLINE */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <float.h> header file. */
#define HAVE_FLOAT_H 1

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define if you have GNU Scientific Library 1.0 or newer */
/* #undef HAVE_GSL_10 */

/* Define if you have GNU Scientific Library 1.4 or newer */
/* #undef HAVE_GSL_14 */

/* Define to 1 if the system has the type `int16_t'. */
#define HAVE_INT16_T 1

/* Define to 1 if the system has the type `int32_t'. */
#define HAVE_INT32_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `m' library (-lm). */
/* #undef HAVE_LIBM */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have a working `mmap' system call. */
/* #undef HAVE_MMAP */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if the system has the type `rlim_t'. */
#define HAVE_RLIM_T 1

/* Define to 1 if you have `size_t' defined */
#define HAVE_SIZE_T 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <sqlite3.h> header file. */
/* #define HAVE_SQLITE3_H 1 */

/* Define to 1 if the system has the type `ssize_t'. */
#define HAVE_SSIZE_T 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcat' function. */
/* #undef HAVE_STRLCAT */

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
#define HAVE_SYS_DIR_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if the system has the type `uint'. */
#define HAVE_UINT 1

/* Define to 1 if the system has the type `uint16_t'. */
#define HAVE_UINT16_T 1

/* Define to 1 if the system has the type `uint32_t'. */
#define HAVE_UINT32_T 1

/* Define to 1 if the system has the type `ulong'. */
#define HAVE_ULONG 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type `u_int16_t'. */
#define HAVE_U_INT16_T 1

/* Define to 1 if the system has the type `u_int32_t'. */
#define HAVE_U_INT32_T 1

/* Define to 1 if the system has the type `u_int8_t'. */
#define HAVE_U_INT8_T 1

/* Define to 1 if you have the <values.h> header file. */
/* #undef HAVE_VALUES_H */

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if the system has the type `_Bool'. */
#if (__CC_NORCROFT == 1) && (__CC_NORCROFT_VERSION >= 544)
# define HAVE__BOOL 1
#endif

/* Define to 1 if trio is to be included. */
/* #undef NEEDTRIO */

/* Name of package */
#define PACKAGE "bogofilter"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "sbellon@sbellon.de"

/* Define to the full name of this package. */
/* #undef PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef PACKAGE_TARNAME */

/* Define to the version of this package. */
/* #undef PACKAGE_VERSION */

/* Define to 1 if the C compiler supports function prototypes. */
#define PROTOTYPES 1

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to the type of arg 1 for `select'. */
#define SELECT_TYPE_ARG1 int

/* Define to the type of args 2, 3 and 4 for `select'. */
#define SELECT_TYPE_ARG234 __fd_set*

/* Define to the type of arg 5 for `select'. */
#define SELECT_TYPE_ARG5 struct timeval*

/* Define to 1 if the `setvbuf' function takes the buffering type as its
   second argument and the buffer pointer as the third, as on System V before
   release 3. */
/* #undef SETVBUF_REVERSED */

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of a `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 4

/* Use specified header name instead of X-Bogosity */
#define SPAM_HEADER_NAME "X-Bogosity"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Version number of package */
/* #undef VERSION */

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1

/* Define __NO_CTYPE to 1 to avoid GLIBC_2.3-specific ctype.h functions. */
/* #undef __NO_CTYPE */

/* Define like PROTOTYPES; this can be used by system headers. */
/* #undef __PROTOTYPES */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to trio_snprintf if your system lacks snprintf */
/* #undef snprintf */

/* Define to `unsigned long' if <sys/types.h> does not define. */
/* #undef u_long */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */

/* Define to empty if the keyword `volatile' does not work. Warning: valid
   code using `volatile' can become incorrect without. Disable with care. */
/* #undef volatile */

/* Define to trio_vsnprintf if your system lacks vsnprintf */
/* #undef vsnprintf */


#ifdef HAVE_EXTENDED_PRECISION_REGISTERS
#define GSL_COERCE_DBL(x) (gsl_coerce_double(x))
#else
#define GSL_COERCE_DBL(x) (x)
#endif
