AC_PREREQ(2.61)
AC_INIT(rpm, 4.8.1, rpm-maint@lists.rpm.org)

AC_CONFIG_SRCDIR([rpmqv.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 foreign tar-ustar dist-bzip2 subdir-objects nostdinc])

AC_CONFIG_TESTDIR(tests)

AC_USE_SYSTEM_EXTENSIONS

AC_DISABLE_STATIC

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_YACC

AC_SUBST(__CC, $CC)
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" = no; then
    AC_MSG_ERROR([ISO C99 capable compiler required])
fi

AS=${AS-as}
AC_SUBST(AS)
if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -fPIC -DPIC -D_REENTRANT -Wall -Wpointer-arith -Wmissing-prototypes -Wno-char-subscripts"
    # XXX disabled for now due to noise from NSPR headers
    # CFLAGS="$CFLAGS -Wstrict-prototypes"
    cflags_to_try="-fno-strict-aliasing -fstack-protector -Wempty-body"
    AC_MSG_CHECKING([supported compiler flags])
    old_cflags=$CFLAGS
    echo
    for flag in $cflags_to_try; do
        CFLAGS="$CFLAGS $flag"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],[
                echo "   $flag"
                RPMCFLAGS="$RPMCFLAGS $flag"
        ],[])
        CFLAGS=$old_cflags
    done
    CFLAGS="$CFLAGS $RPMCFLAGS"
fi
export CFLAGS

AC_SYS_LARGEFILE

dnl
dnl look for POSIX chmod attributes
dnl
AC_MSG_CHECKING(POSIX chmod)
touch foo.chmodtest
chmod 744 foo.chmodtest
chmod +X foo.chmodtest 2>/dev/null
a=`ls -l foo.chmodtest | awk '{print substr($1,1,10)}'`
rm -f foo.chmodtest
if test "$a" = "-rwxr-xr-x"; then
    AC_MSG_RESULT(yes)
    FIXPERMS=a+rX,u+w,g-w,o-w 
else
    AC_MSG_RESULT(no (tell your OS vendor about GNU fileutils))
    FIXPERMS=a+r,u+w,g-w,o-w 
fi
AC_SUBST(FIXPERMS)

dnl
dnl see if we have a mkdir that supports `-p'.
dnl
AC_PATH_PROGS(RPM_MKDIR, mkdir, mkdir)
AC_MSG_CHECKING(if $RPM_MKDIR supports -p)
rm -rf conftest
$RPM_MKDIR -p conftest/a 2>/dev/null
if test $? = 0 ; then
	rmdir conftest/a 2>/dev/null
	if test $? = 0  ; then
		:
	else
		RPM_MKDIR_P=0
	fi

	rmdir conftest 2>/dev/null
	if test $? = 0 ; then
		RPM_MKDIR_P="$RPM_MKDIR -p"
	else
		RPM_MKDIR_P=0
	fi
else
	RPM_MKDIR_P=0
fi

if test X"$RPM_MKDIR_P" = X0 ; then
	AC_MSG_RESULT(no)
	RPM_MKDIR_P="`echo ${prefix}/lib/rpm/mkinstalldirs`"
else
	AC_MSG_RESULT(yes)
fi
dnl
dnl substitute this into config.h, so the C source picks it up.
dnl
AC_DEFINE_UNQUOTED(RPM_MKDIR_P, "${RPM_MKDIR_P}",
[A full path to a program, possibly with arguments, that will create a
   directory and all necessary parent directories, ala 'mkdir -p'])
AC_SUBST(RPM_MKDIR_P)

dnl AM_DISABLE_SHARED
AM_PROG_LIBTOOL

AC_CHECK_TOOL(AR, ar, :)

dnl
dnl use defaults if cross-compiling, otherwise use the default path.
dnl
if test "$cross_compiling" = "yes"; then
    MYPATH=":"
else
    MYPATH=$PATH
fi

dnl
dnl Find some common programs
dnl
AC_PATH_PROG(__BZIP2, bzip2, /usr/bin/bzip2, $MYPATH)
AC_PATH_PROG(__CAT, cat, /bin/cat, $MYPATH)
AC_PATH_PROG(__CHGRP, chgrp, /bin/chgrp, $MYPATH)
AC_PATH_PROG(__CHMOD, chmod, /bin/chmod, $MYPATH)
AC_PATH_PROG(__CHOWN, chown, /bin/chown, $MYPATH)
AC_PATH_PROG(__CP, cp, /bin/cp, $MYPATH)
AC_PATH_PROG(__CPIO, cpio, /bin/cpio, $MYPATH)
AC_PATH_PROG(__CURL, curl, /usr/bin/curl, $MYPATH)
AC_PATH_PROG(__FILE, file, /usr/bin/file, $MYPATH)
AC_PATH_PROG(__GPG, gpg, /usr/bin/gpg, $MYPATH)
AC_PATH_PROG(__GREP, grep, /bin/grep, $MYPATH)
AC_PATH_PROG(__GZIP, gzip, /bin/gzip, $MYPATH)
AC_PATH_PROG(__UNZIP, unzip, /usr/bin/unzip, $MYPATH)

AC_PATH_PROG(__ID, id, /usr/bin/id, $MYPATH)
AC_MSG_CHECKING(checking whether id supports -u)
    if ${__ID} -u 2>&1 > /dev/null ; then
	__ID_U="%{__id} -u"
	AC_MSG_RESULT(yes)
    else
	__ID_U="%{__id} | %{__sed} 's/[[^=]]*=\\\\([[0-9]][[0-9]]*\\\\).*$/\\\\1/'"
	AC_MSG_RESULT(no)
    fi
AC_SUBST(__ID_U)

AC_PATH_PROG(__INSTALL, install, /usr/bin/install, $MYPATH)
AC_PATH_PROG(__XZ, xz, /usr/bin/xz, $MYPATH)
AC_PATH_PROG(__MAKE, make, /usr/bin/make, $MYPATH)
AC_PATH_PROG(__MKDIR, mkdir, /bin/mkdir, $MYPATH)
AC_PATH_PROG(__MV, mv, /bin/mv, $MYPATH)
AC_PATH_PROG(__PATCH, patch, /usr/bin/patch, $MYPATH)
AC_MSG_CHECKING(old version of patch)
    PATCHVERSION=`patch --version 2>&1`

    if test "$PATCHVERSION" = "Patch version 2.1"; then
	AC_DEFINE(HAVE_OLDPATCH_21, 1,
		[Define if the patch call you'll be using is 2.1 or older])
	AC_MSG_RESULT(patch older then 2.2 found)
    else
	AC_MSG_RESULT(patch later then 2.2 found)
    fi

AC_PATH_PROG(__PERL, perl, /usr/bin/perl, $MYPATH)
AC_PATH_PROG(__PGP, pgp, /usr/bin/pgp, $MYPATH)
AC_PATH_PROG(__PYTHON, python, /usr/bin/python, $MYPATH) 
AC_PATH_PROG(__RM, rm, /bin/rm, $MYPATH)
AC_PATH_PROG(__RSH, rsh, /usr/bin/rsh, $MYPATH)
AC_PATH_PROG(__SED, sed, /bin/sed, $MYPATH)
AC_PATH_PROG(__SSH, ssh, /usr/bin/ssh, $MYPATH)
AC_PATH_PROG(__TAR, tar, /bin/tar, $MYPATH)

AC_PATH_PROG(__LD, ld, /usr/bin/ld, $MYPATH)
AC_PATH_PROG(__NM, nm, /usr/bin/nm, $MYPATH)
AC_PATH_PROG(__OBJCOPY, objcopy, /usr/bin/objcopy, $MYPATH)
AC_PATH_PROG(__OBJDUMP, objdump, /usr/bin/objdump, $MYPATH)
AC_PATH_PROG(__STRIP, strip, /usr/bin/strip, $MYPATH)
AC_PATH_PROG(__FAKECHROOT, fakechroot, /usr/bin/fakechroot, $MYPATH)

#=================
# Check for zlib library. 

WITH_ZLIB_INCLUDE=
WITH_ZLIB_LIB=

AC_CHECK_HEADERS([zlib.h],[
  AS_IF([test -z "${WITH_ZLIB_LIB}"],[
    for zlib in z gz ; do
      AC_CHECK_LIB(${zlib}, gzread, 
	[WITH_ZLIB_LIB="-l${zlib}"; break])
    done

dnl zlib-1.0.4 has not gzseek
    AC_CHECK_LIB(${zlib}, gzseek, [AC_DEFINE(HAVE_GZSEEK, 1, [Define as 1 if your zlib has gzseek()])])
  ])
])

AC_SUBST(WITH_ZLIB_INCLUDE)
AC_SUBST(WITH_ZLIB_LIB)

#=================
# Check for bzip2 library.

AC_CHECK_HEADERS([bzlib.h],[
  AC_CHECK_LIB(bz2, bzread, [WITH_BZ2_LIB=-lbz2],
  [
    AC_CHECK_LIB(bz2, BZ2_bzread,[ 
      WITH_BZ2_LIB="-lbz2"
      AC_DEFINE(HAVE_BZ2_1_0, 1, [Define as 1 if you bzip2 1.0])
    ]) 
  ])
])
AC_SUBST(WITH_BZ2_LIB)

#=================
# Check for lzma library.

AC_CHECK_HEADERS([lzma.h],[
  AC_CHECK_LIB(lzma, lzma_easy_encoder, [WITH_LZMA_LIB=-llzma])
])
AC_SUBST(WITH_LZMA_LIB)

#=================

dnl
dnl Check for features
dnl

dnl Checks for libraries.

AC_CHECK_FUNC(setreuid, [], [
    AC_CHECK_LIB(ucb, setreuid, [if echo $LIBS | grep -- -lucb >/dev/null ;then :; else LIBS="$LIBS -lc -lucb" USEUCB=y;fi])
])

AC_CHECK_FUNC(rand, [], [
    AC_CHECK_LIB(rand, rand, [])
])

AC_CHECK_FUNC(getdomainname, [], [
    AC_CHECK_LIB(nsl, getdomainname)
])
AC_CHECK_FUNC(socket, [], [
    AC_CHECK_LIB(socket, socket)
])

AC_CHECK_HEADERS(error.h)
AC_CHECK_FUNCS(error)

AC_CHECK_HEADERS(poll.h)

AC_CHECK_HEADERS(pthread.h)

AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
  dnl OSF 5.0 has the the symbols prefixed with __ in libpthread.
  AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
    AC_CHECK_LIB(thread, mutex_lock)
  ])
])

AC_SEARCH_LIBS(aio_read, [c rt aio posix4])

AC_CHECK_FUNCS(vsnprintf snprintf)

dnl Temporary hack for MiNT.  Some functions (writev, snprintf) are
dnl not in the libc but in libport (for political reasons).  This check
dnl can hopefully be removed soon.  Please use the default action
dnl for this macro (not LIBS=...), otherwise the check for dbopen
dnl will fail.
AC_CHECK_LIB(port, writev)

#=================
# Check for libelf library. Prefer external, otherwise none.
WITH_LIBELF_INCLUDE=
WITH_LIBELF_LIB=
AC_CHECK_HEADER([libelf.h])
AC_CHECK_HEADERS([gelf.h], [
	AC_CHECK_LIB(elf, gelf_getvernaux, [
	    AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the 'elf' library (-lelf).])
	    WITH_LIBELF_LIB="-lelf"
	    WITH_LIBELF=yes
	])
])
AC_SUBST(WITH_LIBELF_INCLUDE)
AC_SUBST(WITH_LIBELF_LIB)
AM_CONDITIONAL(LIBELF,[test "$WITH_LIBELF" = yes])

AC_CHECK_HEADERS([dwarf.h], [
  WITH_LIBDWARF=yes
])
AM_CONDITIONAL(LIBDWARF,[test "$WITH_LIBDWARF" = yes])

#=================
# Check for NSS library.
# We need nss.h from NSS which needs nspr.h. Unfortunately both glibc and NSS 
# have a header named nss.h... so make extra check for NSS's sechash.h 
# which we use too and hopefully is slightly more unique to NSS.
WITH_NSS_INCLUDE=
WITH_NSS_LIB=
AC_CHECK_HEADERS([nspr.h nss.h sechash.h], [], [
  AC_MSG_ERROR([missing required NSPR / NSS header])
])
AC_CHECK_LIB(nss3, NSS_NoDB_Init, [
  WITH_NSS_LIB=-lnss3
], [
  AC_MSG_ERROR([missing required NSS library 'nss3'])
])
AC_SUBST(WITH_NSS_INCLUDE)
AC_SUBST(WITH_NSS_LIB)

#=================
# Check for magic library.
WITH_MAGIC_INCLUDE=
WITH_MAGIC_LIB=

AC_CHECK_HEADER([magic.h], [
    AC_CHECK_LIB(magic, magic_open, [
      WITH_MAGIC_INCLUDE=
      WITH_MAGIC_LIB="-lmagic"
    ],[
      AC_MSG_ERROR([missing required library 'libmagic']) 
    ])
],[
      AC_MSG_ERROR([missing required header magic.h]) 
])

AC_SUBST(WITH_MAGIC_INCLUDE)
AC_SUBST(WITH_MAGIC_LIB)

#=================
# Check for popt library.
WITH_POPT_INCLUDE=
WITH_POPT_LIB=

AC_CHECK_HEADER([popt.h], [
  AC_CHECK_LIB(popt, poptGetContext, [
      WITH_POPT_INCLUDE=
      WITH_POPT_LIB="-lpopt"
  ],[
    AC_MSG_ERROR([missing required library popt])
  ])
],[
  AC_MSG_ERROR([missing required header popt.h])
])

AC_SUBST(WITH_POPT_INCLUDE)
AC_SUBST(WITH_POPT_LIB)

#=================
# Process --with/without-external-db
AC_ARG_WITH(external_db, [AS_HELP_STRING([--with-external-db],[build against an external Berkeley db])],
[case "$with_external_db" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --with-external-db]) ;;
esac],
[with_external_db=no])

case "$with_external_db" in
yes )
  AC_CHECK_HEADERS([db.h],[
    AC_CHECK_TYPE([db_threadid_t],[],
      [AC_MSG_WARN([missing type db_threadid_t])],
      [#include <db.h>])
    save_LIBS="$LIBS"
    AC_CHECK_LIB([db],[db_create],[],[
      AC_MSG_ERROR([missing db_create in libdb])
      ])
    AC_CHECK_LIB([db],[db_env_create],[],[
      AC_MSG_ERROR([missing db_env_create in libdb])
      ])
    AC_CHECK_LIB([db],[db_env_set_func_fsync],[],[
      AC_MSG_ERROR([missing db_env_set_func_fsync in libdb])
      ])
    AC_CHECK_LIB([db],[db_strerror],[],[
      AC_MSG_ERROR([missing db_strerror in libdb])
      ])
    WITH_DB_LIB=-ldb
    LIBS="$save_LIBS"
  ],[
    AC_MSG_ERROR([missing required header db.h])
  ])
  ;;
* ) # Fall back to internal db if available
  if [ test -x db/dist/configure ]; then
    AC_DEFINE(HAVE_DB_H, 1, [Define if you have the <db3/db.h> header file])
  else
    AC_MSG_ERROR([internal Berkeley DB directory not present, see INSTALL])
  fi
  ;;
esac

AC_SUBST([WITH_DB_LIB])

#=================
# Check for sqlite3 library.
AC_ARG_ENABLE(sqlite3, [AS_HELP_STRING([--enable-sqlite3],[enable sqlite3 support])],
[case "$enable_sqlite3" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --enable-sqlite3])
  ;;
esac],
[enable_sqlite3=no])

WITH_SQLITE3_INCLUDE=
WITH_SQLITE3_LIB=
WITH_SQLITE3=no
AS_IF([test "$enable_sqlite3" = yes],[
  AC_CHECK_HEADERS([sqlite3.h],
  [ AC_CHECK_LIB(sqlite3, sqlite3_open, [
      WITH_SQLITE3_INCLUDE=
      WITH_SQLITE3_LIB="-lsqlite3"
      WITH_SQLITE3=yes
    ],[
      AC_MSG_ERROR([missing libsqlite3])
    ])
  ],[
    AC_MSG_ERROR([missing sqlite3.h])
  ])
])

AC_SUBST(WITH_SQLITE3_INCLUDE)
AC_SUBST(WITH_SQLITE3_LIB)
AM_CONDITIONAL([SQLITE3],[test "$WITH_SQLITE3" = yes])

#=================

dnl AmigaOS and IXEmul have a fork() dummy
  case "$host" in
    m68k-*-amigaos ) 
	echo "Building for AmigaOS: using vfork() instead of fork()"; 
	CFLAGS="$CFLAGS -Dfork=vfork" 
	;;
  esac

AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])

dnl Checks for header files we can live without.
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_DIRENT
AC_HEADER_TIME

AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(fcntl.h getopt.h grp.h memory.h netdb.h pwd.h utime.h)

AC_CHECK_HEADERS(sys/ipc.h sys/socket.h sys/select.h)
AC_CHECK_HEADERS(sys/types.h sys/stdtypes.h)
AC_CHECK_HEADERS(sys/mman.h sys/resource.h sys/utsname.h sys/wait.h)

AC_CHECK_HEADERS(netinet/in_systm.h)
AC_CHECK_HEADERS(mntent.h sys/mnttab.h sys/systemcfg.h)
AC_CHECK_HEADERS(sys/mount.h sys/mntctl.h sys/param.h sys/vmount.h)
AC_CHECK_HEADERS(libio.h)
AC_CHECK_HEADERS(err.h mcheck.h)

AC_CHECK_HEADERS(glob.h)

dnl statfs portability fiddles.
dnl
dnl We should really emulate/steal sections of the statfs and struct statfs
dnl checks from GNU fileutils.
dnl
AC_MSG_CHECKING(for struct statfs)

dnl
dnl this is easier than nesting AC_TRY_COMPILEs...
dnl
found_struct_statfs=no

if test X$found_struct_statfs = Xno ; then
dnl Solaris 2.6+ wants to use statvfs
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/statvfs.h> ]], [[struct statvfs sfs;]])],[AC_MSG_RESULT(in sys/statvfs.h)
	AC_DEFINE(STATFS_IN_SYS_STATVFS, 1,
		[statfs in <sys/statvfs.h> (for solaris 2.6+ systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl first try including sys/vfs.h
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/vfs.h> ]], [[struct statfs sfs;]])],[AC_MSG_RESULT(in sys/vfs.h)
	AC_DEFINE(STATFS_IN_SYS_VFS, 1, [statfs in <sys/vfs.h> (for linux systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl ...next try including sys/mount.h
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h> ]], [[struct statfs sfs;]])],[AC_MSG_RESULT(in sys/mount.h)
	AC_DEFINE(STATFS_IN_SYS_MOUNT, 1, [statfs in <sys/mount.h> (for Digital Unix 4.0D systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl ...still no joy.  Try sys/statfs.h
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/statfs.h> ]], [[struct statfs sfs;]])],[AC_MSG_RESULT(in sys/statfs.h)
	AC_DEFINE(STATFS_IN_SYS_STATFS, 1, [statfs in <sys/statfs.h> (for Irix 6.4 systems)])
	found_struct_statfs=yes],[])
fi

if test X$found_struct_statfs = Xno ; then
dnl ...no luck.  Warn the user of impending doom.
AC_MSG_WARN(not found)
fi

dnl
dnl if we found the struct, see if it has the f_bavail member.  Some OSes
dnl don't, including IRIX 6.5+
dnl
if test X$found_struct_statfs = Xyes ; then
AC_MSG_CHECKING(for f_bavail member in struct statfs)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if STATFS_IN_SYS_STATVFS
# include <sys/statvfs.h>
  typedef struct statvfs STATFS_t;
#else
  typedef struct statfs STATFS_t;
# if STATFS_IN_SYS_VFS
#  include <sys/vfs.h>
# elif STATFS_IN_SYS_MOUNT
#  include <sys/mount.h>
# elif STATFS_IN_SYS_STATFS
#  include <sys/statfs.h>
# endif
#endif ]], [[STATFS_t sfs;
	sfs.f_bavail = 0;]])],[AC_MSG_RESULT(yes)
	AC_DEFINE(STATFS_HAS_F_BAVAIL, 1, [Define if struct statfs has the f_bavail member])],[AC_MSG_RESULT(no)
])
fi

if test X$found_struct_statfs = Xyes ; then
dnl
dnl now check to see if we have the 4-argument variant of statfs()
dnl this pretty much requires AC_RUN_IFELSE([AC_LANG_SOURCE([[]])],[],[],[])
dnl
AC_MSG_CHECKING([if statfs() requires 4 arguments])
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef STATFS_IN_SYS_VFS
#include <sys/vfs.h>
#elif STATFS_IN_SYS_MOUNT
#include <sys/mouht.h>
#elif STATFS_IN_SYS_STATFS
#include <sys/statfs.h>
#endif
main() {
	struct statfs sfs;
	exit (statfs(".", &sfs, sizeof(sfs), 0));
}
]])],[AC_MSG_RESULT(yes)
	AC_DEFINE(STAT_STATFS4, 1, [Define if the statfs() call takes 4 arguments])],[AC_MSG_RESULT(no)],[AC_MSG_RESULT(no)
])
fi

AC_C_INLINE

dnl look for libc features
PROVIDES_ERRNO=no
AC_MSG_CHECKING(if <netdb.h> defines h_errno)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]], [[printf("%d",h_errno)]])],[PROVIDES_ERRNO=yes],[])
AC_MSG_RESULT($PROVIDES_ERRNO)
if test $PROVIDES_ERRNO = yes; then
	AC_DEFINE(HAVE_HERRNO, 1, [ Define as 1 if <netdb.h> defines h_errno])
fi

dnl If a system doesn't have S_IFSOCK, define it as 0 which will
dnl make S_ISSOCK always return false (nice, eh?)
AC_MSG_CHECKING(if <sys/stat.h> defines S_IFSOCK)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_IFSOCK)]])],[HAS_S_IFSOCK=yes],[HAS_S_IFSOCK=no])
AC_MSG_RESULT($HAS_S_IFSOCK)
if test $HAS_S_IFSOCK = yes; then
	AC_DEFINE(HAVE_S_IFSOCK, 1, [Define as 1 if <sys/stat.h> defines S_IFSOCK])
fi

dnl Some Unix's are missing S_ISLNK, S_ISSOCK
AC_MSG_CHECKING(if <sys/stat.h> defines S_ISLNK)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_ISLNK(0755))]])],[HAS_S_ISLNK=yes],[HAS_S_ISLNK=no])
AC_MSG_RESULT($HAS_S_ISLNK)
if test $HAS_S_ISLNK = yes; then
	AC_DEFINE(HAVE_S_ISLNK, 1, [Define as 1 if <sys/stat.h> defines S_ISLNK])
fi

AC_MSG_CHECKING(if <sys/stat.h> defines S_ISSOCK)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[printf("%d", S_ISSOCK(0755))]])],[HAS_S_ISSOCK=yes],[HAS_S_ISSOCK=no])
AC_MSG_RESULT($HAS_S_ISSOCK)
if test $HAS_S_ISSOCK = yes; then
	AC_DEFINE(HAVE_S_ISSOCK, 1, [Define as 1 if <sys/stat.h> defines S_ISSOCK])
fi

dnl Check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_FUNC_VPRINTF
dnl XXX don't want to re-enable code that's been unused for years at this
dnl point, but should offer good performance improvements, check after
dnl 4.4.2.1...
dnl AC_FUNC_MMAP

AC_CHECK_FUNCS(getaddrinfo getnameinfo inet_aton)
AC_CHECK_FUNCS(mtrace)
AC_CHECK_FUNCS(strerror)

AC_REPLACE_FUNCS(basename getcwd getwd)
AC_REPLACE_FUNCS(putenv realpath setenv)
AC_REPLACE_FUNCS(stpcpy stpncpy strcspn)
AC_REPLACE_FUNCS(strdup strtol strtoul strspn strstr)

AC_CHECK_FUNCS(__secure_getenv)

AC_CHECK_FUNCS(regcomp)

AC_CHECK_FUNCS(ftok)

AC_CHECK_FUNCS([mkstemp], [], [AC_MSG_ERROR([mkstemp() is required by rpm])])

dnl XXX Glob *is* broken on linux with libc5, solaris and possibly aix when
dnl %files gets something like
dnl	/usr/*/locale/*/LC_MESSAGES/*.mo
dnl (Note: more than one asterisk in glob pattern.)
dnl
dnl XXX Glob is "fixed" in glibc-2.3.3-61, but the cost is that
dnl dangling symlinks are no longer globbed. Always use the internal glob.
AC_DEFINE(USE_GNU_GLOB, 1, [Use the included glob.c?])
AC_LIBOBJ(glob)
AC_LIBOBJ(fnmatch)

dnl check if python is requested
AC_ARG_ENABLE(python, [AS_HELP_STRING([--enable-python],[build rpm python bindings])],
[case "$enable_python" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --enable-python])
  ;;
esac],
[enable_python=no])

AS_IF([test "$enable_python" = yes],[
  AM_PATH_PYTHON([2.6],[
    WITH_PYTHON_INCLUDE=`${PYTHON} -c 'from distutils.sysconfig import *; import sys; sys.stdout.write(get_python_inc())'`
    WITH_PYTHON_SUBPACKAGE=1
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$WITH_PYTHON_INCLUDE"
    AC_CHECK_HEADER([Python.h],[],
      [AC_MSG_ERROR([missing Python.h])
      ])
    CPPFLAGS="$save_CPPFLAGS"
    save_LIBS="$LIBS"
    AC_SEARCH_LIBS([Py_Main],[python${PYTHON_VERSION} python],[
      WITH_PYTHON_LIB="$ac_res"
    ],[AC_MSG_ERROR([missing python library])
    ])
    LIBS="$save_LIBS"
  ])
],[
    WITH_PYTHON_INCLUDE=
    WITH_PYTHON_LIB=
    WITH_PYTHON_SUBPACKAGE=0
])

AC_SUBST(WITH_PYTHON_INCLUDE)
AC_SUBST(WITH_PYTHON_LIB)

AM_CONDITIONAL(PYTHON,[test "$WITH_PYTHON_SUBPACKAGE" = 1])

AC_PATH_PROG(DOXYGEN, doxygen, no)
dnl
dnl Only build internal API + source hacking docs on request
dnl
AC_ARG_WITH(hackingdocs, [AS_HELP_STRING([--with-hackingdocs],[build rpm hacking docs])],,[with_hackingdocs=no])

case "$with_hackingdocs" in
auto)
  AS_IF([test "$DOXYGEN" = no],[with_hackingdocs=no],[with_hackingdocs=no])
  ;;
yes)
  AS_IF([test "$DOXYGEN" = no],
    [AC_MSG_ERROR([rpm hacking docs eeds doxygen in PATH])])
  ;;
esac
AC_CHECK_PROG(HAVE_DOT,[dot],[YES],[NO])

WITH_SELINUX_LIB=
AC_ARG_WITH(selinux, [AS_HELP_STRING([--with-selinux],[build with selinux support])],
[case "$with_selinux" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --with-selinux])
  ;;
esac],
[with_selinux=no])

AS_IF([test "$with_selinux" = yes],[
  AC_CHECK_HEADER([selinux/selinux.h],[
    AC_CHECK_LIB(selinux,[is_selinux_enabled],[with_selinux=yes],[
      AC_MSG_ERROR([--with-selinux given, but libselinux not found])])
  ],[
    AC_MSG_ERROR([--with-selinux given, but selinux/selinux.h not found])
  ])
])

AS_IF([test "$with_selinux" = yes],[
  AC_DEFINE(WITH_SELINUX, 1, [Build with selinux support?])
  WITH_SELINUX_LIB="-lselinux"
])
AC_SUBST(WITH_SELINUX_LIB)
AM_CONDITIONAL(SELINUX,[test "$with_selinux" = yes])

# libcap
WITH_CAP_LIB=
AC_ARG_WITH(cap, [AS_HELP_STRING([--with-cap],[build with capability support])],
[case "$with_cap" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --with-cap])
  ;;
esac],
[with_cap=no])

AS_IF([test "$with_cap" = yes],[
  AC_CHECK_HEADER([sys/capability.h],[
    AC_CHECK_LIB(cap,[cap_get_file], [
      with_cap=yes
      save_LIBS="$LIBS"
      AC_CHECK_LIB(cap,[cap_compare], [
        AC_DEFINE(HAVE_CAP_COMPARE, 1, [Have cap_compare function?])
      ])
      LIBS="$save_LIBS"
    ],[
      AC_MSG_ERROR([--with-cap given, but libcap not found])])
  ],[
    AC_MSG_ERROR([--with-cap given, but sys/capability.h not found])
  ])
])

AS_IF([test "$with_cap" = yes],[
  AC_DEFINE(WITH_CAP, 1, [Build with capability support?])
  WITH_CAP_LIB="-lcap"
])
AC_SUBST(WITH_CAP_LIB)
AM_CONDITIONAL(CAP,[test "$with_cap" = yes])

WITH_ACL_LIB=
AC_ARG_WITH(acl, [AS_HELP_STRING([--with-acl],[build with acl support])],
[case "$with_acl" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --with-acl])
  ;;
esac],
[with_acl=no])

AS_IF([test "$with_acl" = yes],[
  dnl verification uses non-portable acl_equiv_mode() 
  AC_CHECK_HEADER([acl/libacl.h],[
    AC_CHECK_LIB(acl,[acl_equiv_mode],[with_acl=yes],[
      AC_MSG_ERROR([--with-acl given, but libacl not found or not suitable])])
  ],[
    AC_MSG_ERROR([--with-acl given, but acl/libacl.h not found])
  ])
])

AS_IF([test "$with_acl" = yes],[
  AC_DEFINE(WITH_ACL, 1, [Build with acl support?])
  WITH_ACL_LIB="-lacl"
])
AC_SUBST(WITH_ACL_LIB)
AM_CONDITIONAL(ACL,[test "$with_acl" = yes])

WITH_LUA_LIB=
WITH_LUA_INCLUDE=
AC_ARG_WITH(lua, [AS_HELP_STRING([--with-lua],[build with lua support])],,[with_lua=yes])
AS_IF([test "$with_lua" = yes],[
  AC_CHECK_HEADER([lua.h],[
    AC_CHECK_LIB(lua,[luaL_openlibs],[with_lua=yes],[
      AC_MSG_ERROR([--with-lua given, but liblua not found])
    ], 
    [-lm])
  ],[
      AC_MSG_ERROR([--with-lua given, but lua.h not found])
  ])
])

AS_IF([test "$with_lua" = yes],[
  AC_DEFINE(WITH_LUA, 1, [Build with lua support?])
  WITH_LUA_INCLUDE=
  WITH_LUA_LIB="-llua -lm" 
])
AC_SUBST(WITH_LUA_LIB)
AC_SUBST(WITH_LUA_INCLUDE)
AM_CONDITIONAL(WITH_LUAEXT,[test "$with_lua" = yes])

with_dmalloc=no
AC_ARG_WITH(dmalloc, [AS_HELP_STRING([--with-dmalloc],[build with dmalloc debugging support])])
if test "$with_dmalloc" = yes ; then
  AC_DEFINE(DMALLOC, 1, [Build with dmalloc support?])
  LIBS="$LIBS -ldmalloc"
fi

AC_CHECK_FUNCS(setlocale)

AC_CHECK_FUNCS(getpassphrase)

AC_CHECK_FUNC(getmntent, AC_DEFINE(HAVE_GETMNTENT, 1, [Define if you have the getmntent() function]), [
  AC_CHECK_FUNC(mntctl, AC_DEFINE(HAVE_MNTCTL, 1, [Define as 1 if you have mntctl() (only aix?)]),[
    AC_CHECK_FUNC(getmntinfo, AC_DEFINE(HAVE_GETMNTINFO, 1, [Define as 1 if you have getmntinfo() (Mac OS X)]), [
      AC_CHECK_FUNC(getmntinfo_r, AC_DEFINE(HAVE_GETMNTINFO_R, 1, [Define as 1 if you have getmntinfo_r() (only osf?)]), [
        AC_CHECK_LIB(c_r, getmntinfo_r, [LIBS="$LIBS -lc_r"; 
					AC_DEFINE(HAVE_GETMNTINFO_R, 1, [Define as 1 if you have getmntinfo_r() (only osf?)])], [
		 AC_DEFINE([USE_GETMNTENT], 1, [Defined if getmntent replacement is used])
                 AC_LIBOBJ(getmntent)])
      ])
    ])
  ])
])

AC_CHECK_FUNC(lchown,
   [__CHOWN_RHF="%{__chown} -Rhf"
    __CHGRP_RHF="%{__chgrp} -Rhf"
    AC_DEFINE(HAVE_LCHOWN, 1, [Define as 1 if you have lchown()])],
   [__CHOWN_RHF="%{__chown} -Rf"
    __CHGRP_RHF="%{__chgrp} -Rf"
    dnl Does chown() follow symlinks? This should be a good enough test.
    AC_MSG_CHECKING(whether chown() follows symlinks)
    AC_ARG_ENABLE([broken-chown],
            [AS_HELP_STRING([--enable-broken-chown],[this system's chown follows symbolic links])], 
	    result=$enableval, result=unknown)
    if echo "$build" | ${EGREP} "(aix)|(hpux)|(linux)" > /dev/null ; then
	result=yes
    elif echo "$build" | ${EGREP} "(nextstep)" > /dev/null ; then
	result=no
    fi
    if test $result = unknown; then
	if test `${__ID} | cut -f2 -d\= | cut -f1 -d\(` = 0; then
	    rm -f foo bar
	    touch foo
	    ln -s foo bar
	    ${__CHOWN} 10 bar
	    if test `ls -l foo | awk '{print $3}'` != "root"; then
		result=yes
	    else
		result=no
	    fi
	    ${__RM} -f foo bar
	else
	    AC_MSG_CHECKING((cannot check by non-root user))
	    result=no
	fi
    fi

    AC_MSG_RESULT($result)
    if test $result = yes; then
	    AC_DEFINE(CHOWN_FOLLOWS_SYMLINK, 1, [Define as 1 if chown() follows symlinks and you don't have lchown()])
    fi])
AC_SUBST(__CHOWN_RHF)
AC_SUBST(__CHGRP_RHF)

if echo "$host_os" | grep sco > /dev/null; then
	echo "hacking things up for sco"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(HAVE_STRUCT_MNTTAB, 1,
	[Define as 1 if you have "struct mnttab" (only sco?)])
elif echo "$host_os" | grep sunos > /dev/null; then
	echo "hacking things up for sunos"
	CFLAGS="$CFLAGS -D__USE_FIXED_PROTOTYPES__"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(NEED_MYREALLOC, 1, [Define as 1 if we need myrealloc])
	AC_LIBOBJ(myrealloc)
fi

#
# get rid of the 4-th tuple, if config.guess returned "linux-gnu" for host_os
#
host_os_gnu=-gnu
if echo "$host_os" | grep '.*-gnulibc1' > /dev/null ; then
	host_os=`echo "${host_os}" | sed 's/-gnulibc1$//'`
fi
if echo "$host_os" | grep '.*-gnueabi' > /dev/null ; then
	host_os=`echo "${host_os}" | sed 's/-gnueabi$//'`
	host_os_gnu=-gnueabi
fi
if echo "$host_os" | grep '.*-gnu' > /dev/null ; then
	host_os=`echo "${host_os}" | sed 's/-gnu$//'`
fi

changequote(<, >)
host_os_exact="${host_os}"
host_os_major=`echo "${host_os}" | sed 's/\..*$//'`
host_os_noversion=`echo "${host_os}" | sed 's/[0-9]*\..*$//'`
changequote([, ])

rm -f ./find-provides
if test -f ${srcdir}/autodeps/${host_cpu}-${host_os_exact}.prov ; then
	echo "using ${srcdir}/autodeps/${host_cpu}-${host_os_exact}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${host_cpu}-${host_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${host_os_exact}.prov ; then
	echo "using ${srcdir}/autodeps/${host_os_exact}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${host_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${host_os_major}.prov ; then
	echo "using ${srcdir}/autodeps/${host_os_major}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${host_os_major}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${host_os_noversion}.prov ; then
	echo "using ${srcdir}/autodeps/${host_os_noversion}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${host_os_noversion}.prov ./find-provides
else
    echo "*** no default provides information is available for ${host_os_noversion}"
    ln -s ${srcdir}/autodeps/none ./find-provides
fi

rm -f ./find-requires
if test -f ${srcdir}/autodeps/${host_cpu}-${host_os_exact}.req ; then
	echo "using ${srcdir}/autodeps/${host_cpu}-${host_os_exact}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${host_cpu}-${host_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${host_os_exact}.req ; then
	echo "using ${srcdir}/autodeps/${host_os_exact}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${host_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${host_os_major}.req ; then
	echo "using ${srcdir}/autodeps/${host_os_major}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${host_os_major}.req ./find-requires
elif test -f ${srcdir}/autodeps/${host_os_noversion}.req ; then
	echo "using ${srcdir}/autodeps/${host_os_noversion}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${host_os_noversion}.req ./find-requires
else
    echo "*** no default requires information is available for ${host_os_noversion}"
    ln -s ${srcdir}/autodeps/none ./find-requires
fi

dnl Determine the canonical arch-vendor-os for the host machine
case "${host_cpu}" in
*86)		RPMCANONCOLOR=0; RPMCANONARCH=i386 ;;
ia32e*)		RPMCANONCOLOR=3; RPMCANONARCH=ia32e ;;
amd64*)		RPMCANONCOLOR=3; RPMCANONARCH=amd64 ;;
x86_64*)	RPMCANONCOLOR=3; RPMCANONARCH=x86_64 ;;
alpha*)		RPMCANONCOLOR=0; RPMCANONARCH=alpha ;;
sparc64*)	RPMCANONCOLOR=3; RPMCANONARCH=sparc64 ;;
sparc*)		RPMCANONCOLOR=3; RPMCANONARCH=sparc ;;
ia64*)		RPMCANONCOLOR=2; RPMCANONARCH=ia64 ;;
s390x*)		RPMCANONCOLOR=3; RPMCANONARCH=s390x ;;
s390*)		RPMCANONCOLOR=0; RPMCANONARCH=s390 ;;
powerpc64*|ppc64*)	RPMCANONCOLOR=3; RPMCANONARCH=ppc64 ;;
powerpc*|ppc*)	RPMCANONCOLOR=0; RPMCANONARCH=ppc ;;
arm*)		RPMCANONCOLOR=0; RPMCANONARCH=arm ;;
mipsel*)	RPMCANONCOLOR=0; RPMCANONARCH=mipsel ;;
mips*)		RPMCANONCOLOR=0; RPMCANONARCH=mips ;;
m68k*)		RPMCANONCOLOR=0; RPMCANONARCH=m68k ;;
sh3*)		RPMCANONCOLOR=0; RPMCANONARCH=sh3 ;;
sh4*)		RPMCANONCOLOR=0; RPMCANONARCH=sh4 ;;
*)		RPMCANONCOLOR=0; RPMCANONARCH=unknown ;;
esac
case "${host_os_noversion}" in
mint)		RPMCANONARCH=m68kmint ;;
esac
RPMCANONVENDOR="$build_vendor"
case "${build_vendor}" in
unknown|pc|ibm|redhat|pld|mandrake|conectiva|lvr|yellowdog|caos|suse)
	test -f /etc/redhat-release &&		RPMCANONVENDOR=redhat
	test -f /etc/SuSE-release &&		RPMCANONVENDOR=suse
	test -f /etc/pld-release &&		RPMCANONVENDOR=pld
	test -f /etc/mandrake-release &&	RPMCANONVENDOR=mandrake
	test -f /etc/conectiva-release &&	RPMCANONVENDOR=conectiva
	test -f /etc/lvr-release &&		RPMCANONVENDOR=lvr
	test -f /etc/yellowdog-release &&	RPMCANONVENDOR=yellowdog
	test -f /etc/caos-release &&		RPMCANONVENDOR=caos
	;;
esac
RPMCANONOS="$host_os_noversion"
RPMCANONGNU="$host_os_gnu"
AC_SUBST(RPMCANONCOLOR)
AC_SUBST(RPMCANONARCH)
AC_SUBST(RPMCANONVENDOR)
AC_SUBST(RPMCANONOS)
AC_SUBST(RPMCANONGNU)
AC_DEFINE_UNQUOTED([RPMCANONVENDOR],["${RPMCANONVENDOR}"],[canonical vendor])

if test X"$prefix" = XNONE ; then
    usrprefix="$ac_default_prefix"
else
    usrprefix=$prefix
fi

RPMCONFIGDIR="`echo ${usrprefix}/lib/rpm`"
AC_SUBST(RPMCONFIGDIR)

AC_SUBST(OBJDUMP)

if test "$with_external_db" = no; then
    AC_CONFIG_SUBDIRS(db3)
fi

AM_CONDITIONAL([WITH_INTERNAL_DB],[test "$with_external_db" = no])
AM_CONDITIONAL([DOXYGEN],[test "$DOXYGEN" != no])
AM_CONDITIONAL([HACKINGDOCS],[test "$with_hackingdocs" = yes])

AC_PATH_PROG(AUTOM4TE,autom4te,:)

AS_IF([test "$ac_cv_header_stdint_h" = "yes"],[
AC_DEFINE([__RPM_USES_STDINT_H__],[1],[if rpm uses stdint.h])
])

AC_SUBST([dirstamp],[\${am__leading_dot}dirstamp])

AC_CONFIG_FILES([Makefile
	rpmio/Makefile lib/Makefile build/Makefile
	po/Makefile.in scripts/Makefile 
	misc/Makefile 
	doc/Makefile
	python/Makefile
 	luaext/Makefile
 	tests/Makefile
  ])
AC_OUTPUT
