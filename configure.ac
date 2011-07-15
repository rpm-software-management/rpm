AC_PREREQ(2.61)
AC_INIT(rpm, 4.9.1, rpm-maint@lists.rpm.org)

AC_CONFIG_SRCDIR([rpmqv.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 foreign tar-ustar dist-bzip2 subdir-objects nostdinc])

AC_CONFIG_TESTDIR(tests)

AC_USE_SYSTEM_EXTENSIONS

AC_DISABLE_STATIC

PKG_PROG_PKG_CONFIG

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
    cflags_to_try="-fno-strict-aliasing -fstack-protector -Wempty-body -Wno-unused-but-set-variable"
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
AC_PATH_PROG(__INSTALL, install, /usr/bin/install, $MYPATH)
AC_PATH_PROG(__LRZIP, lrzip, /usr/bin/lrzip, $MYPATH)
AC_PATH_PROG(__LZIP, lzip, /usr/bin/lzip, $MYPATH)
AC_PATH_PROG(__XZ, xz, /usr/bin/xz, $MYPATH)
AC_PATH_PROG(__MAKE, make, /usr/bin/make, $MYPATH)
AC_PATH_PROG(__MKDIR, mkdir, /bin/mkdir, $MYPATH)
AC_PATH_PROG(__MV, mv, /bin/mv, $MYPATH)
AC_PATH_PROG(__PATCH, patch, /usr/bin/patch, $MYPATH)
AC_PATH_PROG(__RESTORECON, restorecon, /sbin/restorecon, $MYPATH)
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
AC_PATH_PROG(__SEMODULE, semodule, /usr/bin/semodule, $MYPATH)
AC_PATH_PROG(__SSH, ssh, /usr/bin/ssh, $MYPATH)
AC_PATH_PROG(__TAR, tar, /bin/tar, $MYPATH)

AC_PATH_PROG(__LD, ld, /usr/bin/ld, $MYPATH)
AC_PATH_PROG(__NM, nm, /usr/bin/nm, $MYPATH)
AC_PATH_PROG(__OBJCOPY, objcopy, /usr/bin/objcopy, $MYPATH)
AC_PATH_PROG(__OBJDUMP, objdump, /usr/bin/objdump, $MYPATH)
AC_PATH_PROG(__STRIP, strip, /usr/bin/strip, $MYPATH)
AC_PATH_PROG(__FAKECHROOT, fakechroot, /usr/bin/fakechroot, $MYPATH)

dnl see if we have a mkdir that supports `-p' for rpmbuild's purposes
AC_PROG_MKDIR_P
if ! echo $MKDIR_P | grep -q mkdir; then 
  __MKDIR_P="`echo ${prefix}/lib/rpm/mkinstalldirs`"
else
  __MKDIR_P=$MKDIR_P
fi
AC_DEFINE_UNQUOTED(__MKDIR_P, "${__MKDIR_P}",
[A full path to a program, possibly with arguments, that will create a
   directory and all necessary parent directories, ala 'mkdir -p'])
AC_SUBST(__MKDIR_P)

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
],[
  AC_MSG_ERROR([missing required header zlib.h])
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

AC_CHECK_FUNC(gethostname, [], [
    AC_CHECK_LIB(nsl, gethostname)
])

AC_CHECK_HEADERS(poll.h)

AC_CHECK_HEADERS(pthread.h)

AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
  dnl OSF 5.0 has the the symbols prefixed with __ in libpthread.
  AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
    AC_CHECK_LIB(thread, mutex_lock)
  ])
])

AC_SEARCH_LIBS(aio_read, [c rt aio posix4])
AC_SEARCH_LIBS(dlopen, [dl])

dnl Temporary hack for MiNT.  Some functions (writev, snprintf) are
dnl not in the libc but in libport (for political reasons).  This check
dnl can hopefully be removed soon.  Please use the default action
dnl for this macro (not LIBS=...), otherwise the check for dbopen
dnl will fail.
AC_CHECK_LIB(port, writev)

#=================
# Check for libelf library. Prefer external, otherwise none.
WITH_LIBELF_LIB=
AC_CHECK_HEADER([libelf.h])
AC_CHECK_HEADERS([gelf.h], [
	AC_CHECK_LIB(elf, gelf_getvernaux, [
	    AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the 'elf' library (-lelf).])
	    WITH_LIBELF_LIB="-lelf"
	    WITH_LIBELF=yes
	])
])
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
    AC_PREPROC_IFELSE([
      AC_LANG_SOURCE([
	#include <db.h>
	#if ((DB_VERSION_MAJOR < 4) || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 5))
	#error Berkeley DB too old
	#endif
      ])
    ],[ WITH_DB_LIB=-ldb ], 
      [ AC_MSG_ERROR([Berkeley DB version >= 4.5 required])
    ])
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

AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])

dnl Checks for header files we can live without.
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_STRUCT_DIRENT_D_TYPE

AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(fcntl.h getopt.h memory.h)

AC_CHECK_HEADERS(sys/ipc.h)
AC_CHECK_HEADERS(sys/utsname.h)

AC_CHECK_HEADERS(sys/systemcfg.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(libio.h)
AC_CHECK_HEADERS(mcheck.h)

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

dnl look for libc features

dnl Check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_FUNC_MMAP

AC_CHECK_FUNCS(mtrace)
AC_CHECK_FUNCS(putenv)

AC_REPLACE_FUNCS(stpcpy stpncpy)

AC_CHECK_FUNCS(__secure_getenv)

AC_CHECK_FUNCS(
   [mkstemp getcwd basename dirname realpath setenv unsetenv regcomp lchown],
   [], [AC_MSG_ERROR([function required by rpm])])

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

AM_CONDITIONAL(ENABLE_PYTHON,[test "$WITH_PYTHON_SUBPACKAGE" = 1])

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
    save_LIBS="$LIBS"
    AC_CHECK_LIB([selinux],[is_selinux_enabled],[],[
      AC_MSG_ERROR([--with-selinux given, but is_selinux_enabled not found in libselinux])])
    AC_CHECK_LIB([selinux],[selinux_getpolicytype],[],[
      AC_MSG_ERROR([--with-selinux given, but selinux_getpolicytype not found in libselinux])])
    AC_CHECK_LIB([selinux],[selinux_reset_config],[],[
      AC_MSG_ERROR([--with-selinux given, but selinux_reset_config not found in libselinux])])
    LIBS="$save_LIBS"
  ],[
    AC_MSG_ERROR([--with-selinux given, but selinux/selinux.h not found])
  ])

  AC_CHECK_HEADER([selinux/label.h],[
    save_LIBS="$LIBS"
    AC_CHECK_LIB([selinux],[selabel_close],[],[
      AC_MSG_ERROR([--with-selinux given, but selabel_close not found in libselinux])])
    AC_CHECK_LIB([selinux],[selabel_lookup_raw],[],[
      AC_MSG_ERROR([--with-selinux given, but selabel_lookup_raw not found in libselinux])])
    AC_CHECK_LIB([selinux],[selabel_open],[],[
      AC_MSG_ERROR([--with-selinux given, but selabel_open not found in libselinux])])
    LIBS="$save_LIBS"
  ],[
    AC_MSG_ERROR([--with-selinux given, but selinux/label.h not found])
  ])

  dnl FIXME: semanage is only needed for the sepolicy plugin
  AC_CHECK_HEADER([semanage/semanage.h],[
    save_LIBS="$LIBS"
    AC_CHECK_LIB([semanage],[semanage_begin_transaction],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_begin_transaction missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_commit],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_commit missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_connect],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_connect missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_disconnect],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_disconnect missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_handle_create],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_handle_create missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_handle_destroy],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_handle_destroy missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_is_connected],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_is_connected missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_module_install_base_file],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_module_install_base_file missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_module_install_file],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_module_install_file missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_module_remove],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_module_remove missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_select_store],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_select_store missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_set_check_contexts],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_set_check_contexts missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_set_create_store],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_set_create_store missing in libsemanage])])
    AC_CHECK_LIB([semanage],[semanage_set_reload],[],[
      AC_MSG_ERROR([--with-selinux given, but semanage_set_reload missing in libsemanage])])
    LIBS="$save_LIBS"
  ],[
    AC_MSG_ERROR([--with-selinux given, but semanage/semanage.h not found])
  ])
])

AS_IF([test "$with_selinux" = yes],[
  AC_DEFINE(WITH_SELINUX, 1, [Build with selinux support?])
  WITH_SELINUX_LIB="-lselinux"
  WITH_SEMANAGE_LIB="-lsemanage"
])
AC_SUBST(WITH_SELINUX_LIB)
AC_SUBST(WITH_SEMANAGE_LIB)
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

AC_ARG_WITH([lua], [AS_HELP_STRING([--with-lua], [build with lua support])],
            [],
            [with_lua=yes])

AS_IF([test "$with_lua" != no],[
  PKG_CHECK_MODULES([LUA],
    [lua >= 5.1],
    [AC_DEFINE(WITH_LUA, 1, [Build with lua support?])],
    [AC_MSG_ERROR([lua not present (--without-lua to disable)])])
  AC_SUBST(LUA_CFLAGS)
  AC_SUBST(LUA_LIBS)
])
AM_CONDITIONAL(WITH_LUA,[test "$with_lua" = yes])

AC_ARG_ENABLE(plugins, [AS_HELP_STRING([--disable-plugins],[build without plugin support])],,[enable_plugins=yes])
AS_IF([test "$enable_plugins" = yes],[
  AC_DEFINE(ENABLE_PLUGINS, 1, [Build with plugin support?])
])
AM_CONDITIONAL(ENABLE_PLUGINS,[test "$enable_plugins" = yes])


with_dmalloc=no
AC_ARG_WITH(dmalloc, [AS_HELP_STRING([--with-dmalloc],[build with dmalloc debugging support])])
if test "$with_dmalloc" = yes ; then
  AC_DEFINE(DMALLOC, 1, [Build with dmalloc support?])
  LIBS="$LIBS -ldmalloc"
fi

AC_CHECK_FUNCS(getpassphrase)

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

AC_SUBST([dirstamp],[\${am__leading_dot}dirstamp])

AC_CONFIG_FILES([Makefile
	rpmio/Makefile lib/Makefile build/Makefile sign/Makefile
	po/Makefile.in scripts/Makefile fileattrs/Makefile
	misc/Makefile 
	doc/Makefile
	python/Makefile
 	luaext/Makefile
 	tests/Makefile
 	plugins/Makefile
  ])
AC_OUTPUT
