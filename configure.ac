dnl configure.in for RPM
dnl
dnl Don't add checks for things we can't work around, unless those checks
dnl failing causes the script to bomb out with a message. If we can't fix
dnl it, why check it?

AC_INIT(rpmqv.c)
AC_CANONICAL_SYSTEM
AC_PREREQ(2.12)         dnl Minimum Autoconf version required.
AC_CONFIG_HEADERS
AM_INIT_AUTOMAKE(rpm, 4.2)
AM_CONFIG_HEADER(config.h)

dnl XXX AM_MAINTAINER_MODE

dnl Set of available languages.
ALL_LINGUAS="cs da de fi fr gl is ja ko no pl pt pt_BR ro ru sk sl sr sv tr"

dnl echo "
dnl ****************************************************************************
dnl *                                                                          *
dnl *                   *** WARNING WARNING WARNING ***                        *
dnl *                                                                          *
dnl * This is source code from the development branch of rpm-4.2.              *
dnl *                                                                          *
dnl * If you want the "production" rpm-4.0.4 or rpm-4.1 code, then you should  *
dnl * either use an rpm-4.0.4 or rpm-4.1 src.rpm. Alternatively, if using a    *
dnl * CVS checkout, do the following:                                          *
dnl *                                                                          *
dnl *       cvs -d :pserver:anonymous@cvs.rpm.org:/cvs/devel login             *
dnl *       (no password, just carriage return)                                *
dnl *       cvs -d :pserver:anonymous@cvs.rpm.org:/cvs/devel get rpm           *
dnl *       cd rpm                                                             *
dnl *                                                                          *
dnl * Here's the rpm-4_1 branch:                                               *
dnl *       cvs up -r rpm-4_1                                                  *
dnl *                                                                          *
dnl * Here's the rpm-4_0 branch:                                               *
dnl *       cvs up -r rpm-4_0                                                  *
dnl *                                                                          *
dnl ****************************************************************************
dnl "
dnl sleep 10

dnl Checks for programs.
AC_PROG_CC
AC_AIX
AC_MINIX
AC_PROG_CXX
AS=${AS-as}
AC_SUBST(AS)
if test "$ac_cv_prog_gcc" = yes; then
    CFLAGS="$CFLAGS -D_GNU_SOURCE -D_REENTRANT -Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts"
fi
export CFLAGS

AC_PROG_CPP
AC_PROG_GCC_TRADITIONAL
AC_SYS_LARGEFILE

dnl Does this platform require array notation to assign to a va_list?
dnl If cross-compiling, we assume va_list is "normal".  If this breaks
dnl you, set ac_cv_valistisarray=true and maybe define HAVE_VA_LIST_AS_ARRAY
dnl also just to be sure.
AC_MSG_CHECKING(whether va_list assignments need array notation)
AC_CACHE_VAL(ac_cv_valistisarray,
	[AC_TRY_RUN([#include <stdlib.h>
		     #include <stdarg.h>
		     void foo(int i, ...) {
			va_list ap1, ap2;
			va_start(ap1, i);
			ap2 = ap1;
			if (va_arg(ap2, int) != 123 || va_arg(ap1, int) != 123)
			    { exit(1); }
			va_end(ap1); va_end(ap2);
		     }
		     int main() { foo(0, 123); return(0); }],
		    [ac_cv_valistisarray=false],
		    [ac_cv_valistisarray=true],
		    [ac_cv_valistisarray=false])])

if test "$ac_cv_valistisarray" = true ; then
	AC_DEFINE(HAVE_VA_LIST_AS_ARRAY, 1,
		[Define as 1 if your va_list type is an array])
	AC_MSG_RESULT(yes)
else
       AC_MSG_RESULT(no)
fi

RPMUSER=rpm
RPMUID=37
RPMGROUP=rpm
RPMGID=37
export RPMUSER RPMUID RPMGROUP RPMGID
AC_SUBST(RPMUSER)
AC_SUBST(RPMUID)
AC_SUBST(RPMGROUP)
AC_SUBST(RPMGID)

AC_PATH_PROG(CTAGS, ctags, /bin/true)
AC_PATH_PROG(CSCOPE, cscope, /bin/true)

dnl
dnl This now uses libtool. Put
dnl	LDFLAGS_STATIC="-all"
dnl to attempt static executables using libtool. Otherwise
dnl	LDFLAGS_STATIC=""
dnl
AC_MSG_CHECKING(flag used by libtool to link rpm)
if test X"$GCC" = Xyes ; then
	case "$target" in
		*-*-solaris*)	LDFLAGS_STATIC="-static";;
		*-*-hpux*)	LDFLAGS_STATIC="-static";;
		*-*-sysv5uw*)   LDFLAGS_STATUS="-static";; # Unixware has no shared libthread.
		*-*-*)		LDFLAGS_STATIC="-all-static";;
	esac
elif test X"$CC" = Xcc ; then
	case "$target" in
		*-*-linux*)	LDFLAGS_STATIC="-all-static";;
		*-*-freebsd*)	LDFLAGS_STATIC="-all-static";;
		*-*-osf*)	LDFLAGS_STATIC="";; # OSF5 has no shared pthreads libs
		*-*-aix*)	LDFLAGS_STATIC="-static";;  # -Wl,-bnso doesn't seem to work...
		*-*-hpux*)	LDFLAGS_STATIC="-static";;
		*-*-solaris*)	LDFLAGS_STATIC="-static";;
		*-*-irix*)	LDFLAGS_STATIC="-static";;  #should be -non_shared, but can't
											# link because of crt1.o then.
		*-*-ultrix*)	LDFLAGS_STATIC="-all-static";;  #ultrix doesn't have shared libs.
		*-*-*)		LDFLAGS_STATIC="-static"
AC_MSG_WARN([

Unable to guess what option to pass to $CC to generate a static
executable.  You will need to set the LDFLAGS_STATIC macro in Makefile.inc to
the appropriate argument(s) if you want to build a static rpm executable.

])
		;;
	esac
else
	# just link it dynamically
	LDFLAGS_STATIC=""
fi
LDFLAGS_STATIC="${LDFLAGS} ${LDFLAGS_STATIC}"	# libtool format
AC_MSG_RESULT($LDFLAGS_STATIC)
AC_SUBST(LDFLAGS_STATIC)

dnl
dnl XXX Test for libpthread.a that is NPTL aware (static link only).
dnl
LDFLAGS_NPTL=
if test -f /usr/lib/nptl/libpthread.a ; then
    LDFLAGS_NPTL="-L/usr/lib/nptl"
#    INCPATH="$INCPATH -I/usr/include/nptl"
fi
AC_SUBST(LDFLAGS_NPTL)

dnl
dnl look for POSIX chmod attributes
dnl
AC_MSG_CHECKING(POSIX chmod)
touch foo.chmodtest
chmod 744 foo.chmodtest
chmod +X foo.chmodtest 2>/dev/null
a=`ls -l foo.chmodtest | awk '{print $1}'`
rm -f foo.chmodtest
if test "$a" = "-rwxr-xr-x"; then
    AC_MSG_RESULT(yes)
    FIXPERMS=a+rX,g-w,o-w 
else
    AC_MSG_RESULT(no (tell your OS vendor about GNU fileutils))
    FIXPERMS=a+r,g-w,o-w 
fi
AC_SUBST(FIXPERMS)

dnl
dnl see if we have a mkdir that supports `-p'.
dnl
AC_PATH_PROGS(MKDIR, mkdir, mkdir)
AC_MSG_CHECKING(if $MKDIR supports -p)
rm -rf conftest
$MKDIR -p conftest/a 2>/dev/null
if test $? = 0 ; then
	rmdir conftest/a 2>/dev/null
	if test $? = 0  ; then
		:
	else
		MKDIR_P=0
	fi

	rmdir conftest 2>/dev/null
	if test $? = 0 ; then
		MKDIR_P="$MKDIR -p"
	else
		MKDIR_P=0
	fi
else
	MKDIR_P=0
fi

if test X"$MKDIR_P" = X0 ; then
	AC_MSG_RESULT(no)
	MKDIR_P="`echo ${prefix}/lib/rpm/mkinstalldirs`"
else
	AC_MSG_RESULT(yes)
fi
dnl
dnl substitute this into config.h, so the C source picks it up.
dnl
AC_DEFINE_UNQUOTED(MKDIR_P, "${MKDIR_P}",
[A full path to a program, possibly with arguments, that will create a
   directory and all necessary parent directories, ala `mkdir -p'])
AC_SUBST(MKDIR_P)

AC_ISC_POSIX

dnl This test must precede tests of compiler characteristics like
dnl that for the inline keyword, since it may change the degree to
dnl which the compiler supports such features.
AM_C_PROTOTYPES

AC_PROG_AWK
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl AM_DISABLE_SHARED
AM_PROG_LIBTOOL

AC_CHECK_TOOL(AR, ar, :)

dnl
dnl use defaults if cross-compiling, otherwise use the default path.
dnl
if test "$cross_compiling" = "yes"; then
    MYPATH=""
else
#    MYPATH="/bin:/usr/bin:/usr/local/bin:$PATH:/sbin:/usr/sbin:/usr/local/sbin:/opt/gnu/bin"
    MYPATH=$PATH
fi

dnl
dnl Find some common programs
dnl
AC_PATH_PROG(BZIP2BIN, bzip2, /usr/bin/bzip2, $MYPATH)
AC_PATH_PROG(__CAT, cat, /bin/cat, $MYPATH)
AC_PATH_PROG(__CHGRP, chgrp, /bin/chgrp, $MYPATH)
AC_PATH_PROG(__CHMOD, chmod, /bin/chmod, $MYPATH)
AC_PATH_PROG(__CHOWN, chown, /bin/chown, $MYPATH)
AC_PATH_PROG(__CP, cp, /bin/cp, $MYPATH)
AC_PATH_PROG(__CPIO, cpio, /bin/cpio, $MYPATH)
AC_PATH_PROG(__FILE, file, /usr/bin/file, $MYPATH)
AC_PATH_PROG(__GPG, gpg, /usr/bin/gpg, $MYPATH)
AC_PATH_PROG(__GREP, grep, /bin/grep, $MYPATH)
AC_PATH_PROG(GZIPBIN, gzip, /bin/gzip, $MYPATH)
AC_PATH_PROG(UNZIPBIN, unzip, /usr/bin/unzip, $MYPATH)

AC_PATH_PROG(__ID, id, /usr/bin/id, $MYPATH)
AC_MSG_CHECKING(checking whether id supports -u)
    if ${__ID} -u 2>&1 > /dev/null ; then
	__ID_U="%{__id} -u"
	AC_MSG_RESULT(yes)
    else
dnl XXX Watchout for square brackets fed to m4.
	__ID_U="%{__id} | %{__sed} 's/[[^=]]*=\\\\([[0-9]][[0-9]]*\\\\).*$/\\\\1/'"
	AC_MSG_RESULT(no)
    fi
AC_SUBST(__ID_U)

AC_PATH_PROG(__INSTALL, install, /usr/bin/install, $MYPATH)
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
AC_PATH_PROG(PGPBIN, pgp, /usr/bin/pgp, $MYPATH)
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

addlib() {
  l=$1
  shift
  case "$target" in 
    *-*-solaris*)	LIBS="$LIBS -L$l -R$l $*";;
    *)		LIBS="$LIBS -L$l $*";;
  esac
}

WITH_ZLIB_SUBDIR=
WITH_ZLIB_INCLUDE=
WITH_ZLIB_LIB=
if test -d zlib ; then
  WITH_ZLIB_SUBDIR=zlib
  addlib \${top_builddir}/zlib
  WITH_ZLIB_INCLUDE="-I\${top_srcdir}/${WITH_ZLIB_SUBDIR}"
  INCPATH="$INCPATH -I\${top_srcdir}/${WITH_ZLIB_SUBDIR}"
  WITH_ZLIB_LIB="\${top_builddir}/${WITH_ZLIB_SUBDIR}/libz.la"
  AC_DEFINE(HAVE_GZSEEK, 1, [Define as 1 if your zlib has gzseek()])
fi
AC_SUBST(WITH_ZLIB_SUBDIR)
AC_SUBST(WITH_ZLIB_INCLUDE)
AC_SUBST(WITH_ZLIB_LIB)

localdone=

dirs=$prefix
if test "$cross_compiling" != "yes"; then
  dirs="$dirs /usr/local"
fi
for dir in $dirs
do
	case $dir in
	NONE|/usr)	continue;;
	/usr/local)
		if test X$localdone != X ; then continue; fi
		localdone="$dir"
		;;
	esac

	AC_MSG_CHECKING(for $dir/lib in LIBS)
	if test -d $dir/lib 2> /dev/null ; then
		addlib $dir/lib
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi

	AC_MSG_CHECKING(for $dir/include in INCPATH)
	if test -d $dir/include 2>/dev/null ; then
		if [ "$dir" != "/usr/local" ] ; then
		    INCPATH="$INCPATH -I$dir/include"
		fi
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
done

AC_MSG_CHECKING(for /usr/ucblib in LIBS)
if test -d /usr/ucblib ; then
	if test "$build" = "mips-sni-sysv4" ; then
		addlib /usr/ccs/lib -lc
	fi

	addlib /usr/ucblib
	
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

dnl
dnl Check for features
dnl

dnl Checks for libraries.

WITH_ELFUTILS_SUBDIR=
WITH_LIBELF_INCLUDE=
WITH_LIBELF_LIB=
AC_CHECK_HEADER([libelf.h])
AC_CHECK_HEADER([gelf.h], [
	AC_DEFINE(HAVE_GELF_H, 1, [Define to 1 if you have the <gelf.h> header file.])
	AC_CHECK_LIB(elf, gelf_getvernaux, [
	    AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the `elf' library (-lelf).])
	    WITH_LIBELF_LIB="-lelf"
	])
    ], [
	if test -d elfutils/libelf ; then
	    AC_DEFINE(HAVE_GELF_H, 1, [Define to 1 if you have the <gelf.h> header file.])
	    AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the `elf' library (-lelf).])
	    WITH_ELFUTILS_SUBDIR=elfutils
	    WITH_LIBELF_INCLUDE="-I\${top_srcdir}/${WITH_ELFUTILS_SUBDIR}/libelf"
	    WITH_LIBELF_LIB="\${top_builddir}/${WITH_ELFUTILS_SUBDIR}/libelf/libelf.a"
	fi
])
AC_SUBST(WITH_ELFUTILS_SUBDIR)
AC_SUBST(WITH_LIBELF_INCLUDE)
AC_SUBST(WITH_LIBELF_LIB)

WITH_LIBDWARF_INCLUDE=
AC_CHECK_HEADER([dwarf.h], [
	AC_DEFINE(HAVE_DWARF_H, 1, [Define to 1 if you have the <dwarf.h> header file.])
    ], [
	if test -d elfutils/libdwarf ; then
	    AC_DEFINE(HAVE_DWARF_H, 1, [Define to 1 if you have the <dwarf.h> header file.])
	    WITH_LIBDWARF_INCLUDE="-I\${top_srcdir}/${WITH_ELFUTILS_SUBDIR}/libdwarf"
	fi
])
AC_SUBST(WITH_LIBDWARF_INCLUDE)

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

AC_CHECK_HEADERS(thread.h pthread.h synch.h semaphore.h)

AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
  dnl OSF 5.0 has the the symbols prefixed with __ in libpthread.
  AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
    AC_CHECK_LIB(thread, mutex_lock)
  ])
])

AC_CHECK_HEADERS(aio.h)
AC_SEARCH_LIBS(aio_read, [c rt aio posix4])

dnl Better not use fchmod at all.
AC_CHECK_FUNC(fchmod)

AC_CHECK_FUNCS(vsnprintf snprintf)

dnl Temporary hack for MiNT.  Some functions (writev, snprintf) are
dnl not in the libc but in libport (for political reasons).  This check
dnl can hopefully be removed soon.  Please use the default action
dnl for this macro (not LIBS=...), otherwise the check for dbopen
dnl will fail.
AC_CHECK_LIB(port, writev)

WITH_DB_SUBDIR=
WITH_INTERNAL_DB=0
DBLIBSRCS=""
libdb3=""
libdb3a=""

dnl
dnl Detect whether internal Berkeley DB should be built.
dnl
withval=yes
AC_ARG_WITH(db, [  --without-db            do not use internal Berkeley db])

if test $withval = no ; then
dnl ------------------ without internal db

AC_CHECK_HEADERS(db3/db.h)

dnl Check for Berkeley db3 API.
AC_CHECK_FUNC(db_create, [DBLIBSRCS="$DBLIBSRCS db3.c"],
  AC_CHECK_LIB(db-3.2, db_create, [DBLIBSRCS="$DBLIBSRCS db3.c"; libdb3="-ldb-3.2"],
    AC_CHECK_LIB(db-3.1, db_create, [DBLIBSRCS="$DBLIBSRCS db3.c"; libdb3="-ldb-3.1"],
      AC_CHECK_LIB(db-3.0, db_create, [DBLIBSRCS="$DBLIBSRCS db3.c"; libdb3="-ldb-3.0"],
        AC_CHECK_LIB(db, db_create, [DBLIBSRCS="$DBLIBSRCS db3.c"; libdb3="-ldb"],
        )
      )
    )
  )
)

if test X"$DBLIBSRCS" = X; then
    AC_MSG_ERROR([sorry rpm requires libdb-3.x.a (from the Berkeley db package)]) 
fi

if test -n "$libdb3" -a -n "$libthread" ; then
   AC_MSG_CHECKING(whether $libdb3 needs $libthread)
   saveLIBS="$LIBS"
   LIBS="$LIBS $libdb3"
   AC_TRY_LINK_FUNC(db_create, AC_MSG_RESULT(no),
	[libdb3="$libdb3 $libthread"; AC_MSG_RESULT(yes)])
   LIBS="$saveLIBS"
fi

else
dnl ------------------ with    internal db
  AC_DEFINE(HAVE_DB3_DB_H, 1, [Define if you have the <db3/db.h> header file])
  WITH_DB_SUBDIR=db3
  WITH_INTERNAL_DB=1
  DBLIBSRCS="db3.c"
  libdb3="# \$(top_builddir)/db3/libdb.la"
  libdb3a="\$(top_builddir)/db3/libdb.a"
dnl  INCPATH="-I\$(top_builddir)/$(WITH_DB_SUBDIR) $INCPATH"
fi

AC_SUBST(WITH_DB_SUBDIR)
AC_SUBST(WITH_INTERNAL_DB)

DBLIBSRCS="$DBLIBSRCS"
DBLIBOBJS=`echo $DBLIBSRCS | sed -e "s/\.c/\.lo/g"`

AC_SUBST(DBLIBSRCS)
AC_SUBST(DBLIBOBJS)
AC_SUBST(libdb3)
AC_SUBST(libdb3a)

for dbi in $DBLIBSRCS; do
  case $dbi in
  db3.c) AC_DEFINE(USE_DB3, 1, [Use the Berkeley db3 API?]) ;;
  db1.c) AC_DEFINE(USE_DB1, 1, [Use the Berkeley db1 retrofit to db3 API?]) ;;
  esac
done

dnl AmigaOS and IXEmul have a fork() dummy
  case "$target" in
    m68k-*-amigaos ) 
	echo "Building for AmigaOS: using vfork() instead of fork()"; 
	CFLAGS="$CFLAGS -Dfork=vfork" 
	;;
  esac

if test -z "${WITH_ZLIB_LIB}" ; then
for zlib in z gz ; do
   AC_CHECK_LIB(${zlib}, gzread, 
	[LIBS="$LIBS -l${zlib}"; break], 
	[if test ${zlib} = gz; then 
	    AC_MSG_ERROR([sorry rpm requires libz.a or libgz.a (from the zlib package)]) 
	 fi]
	       )
done

dnl zlib-1.0.4 has not gzseek
AC_CHECK_LIB(${zlib}, gzseek, [AC_DEFINE(HAVE_GZSEEK, 1, [Define as 1 if your zlib has gzseek()])])
fi

AC_CHECK_LIB(bz2, bzread, [LIBS="$LIBS -lbz2"], 
   AC_CHECK_LIB(bz2, BZ2_bzread, [
     WITH_BZIP2=1
     LIBS="$LIBS -lbz2"
     AC_DEFINE(HAVE_BZ2_1_0, 1, [Define as 1 if you bzip2 1.0]) ], 
     WITH_BZIP2=0))
AC_SUBST(WITH_BZIP2)

AM_GNU_GETTEXT
dnl TVM:
dnl horrible *temporary* hack to make sure that if we found gettext() in
dnl -lintl that we add -lintl *back* to $LIBS.
dnl
if test X$gt_cv_func_gettext_libintl = Xyes ; then
    LIBS="-lintl $LIBS"
fi

dnl Checks for header files we can live without.
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_DIRENT
AC_HEADER_TIME

AC_CHECK_HEADERS(fcntl.h getopt.h grp.h memory.h netdb.h pwd.h utime.h)

AC_CHECK_HEADERS(sys/ipc.h sys/socket.h sys/select.h)
AC_CHECK_HEADERS(sys/types.h sys/stdtypes.h)
AC_CHECK_HEADERS(sys/mman.h sys/resource.h sys/utsname.h sys/wait.h)

AC_CHECK_HEADERS(netinet/in_systm.h)
AC_CHECK_HEADERS(machine/types.h)
AC_CHECK_HEADERS(mntent.h sys/mnttab.h sys/systemcfg.h)
AC_CHECK_HEADERS(sys/mount.h sys/mntctl.h sys/param.h sys/vmount.h)
AC_CHECK_HEADERS(bzlib.h libio.h zlib.h)
AC_CHECK_HEADERS(err.h mcheck.h)
AC_CHECK_HEADERS(pthread.h)

dnl popt w float/double needs.
AC_CHECK_HEADERS(float.h)

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
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/statvfs.h> ],
	[struct statvfs sfs;],
	[AC_MSG_RESULT(in sys/statvfs.h)
	AC_DEFINE(STATFS_IN_SYS_STATVFS, 1,
		[statfs in <sys/statvfs.h> (for solaris 2.6+ systems)])
	found_struct_statfs=yes],
)
fi

if test X$found_struct_statfs = Xno ; then
dnl first try including sys/vfs.h
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/vfs.h> ],
	[struct statfs sfs;],
	[AC_MSG_RESULT(in sys/vfs.h)
	AC_DEFINE(STATFS_IN_SYS_VFS, 1, [statfs in <sys/vfs.h> (for linux systems)])
	found_struct_statfs=yes],
)
fi

if test X$found_struct_statfs = Xno ; then
dnl ...next try including sys/mount.h
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h> ],
	[struct statfs sfs;],
	[AC_MSG_RESULT(in sys/mount.h)
	AC_DEFINE(STATFS_IN_SYS_MOUNT, 1, [statfs in <sys/mount.h> (for Digital Unix 4.0D systems)])
	found_struct_statfs=yes],
)
fi

if test X$found_struct_statfs = Xno ; then
dnl ...still no joy.  Try sys/statfs.h
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/statfs.h> ],
	[struct statfs sfs;],
	[AC_MSG_RESULT(in sys/statfs.h)
	AC_DEFINE(STATFS_IN_SYS_STATFS, 1, [statfs in <sys/statfs.h> (for Irix 6.4 systems)])
	found_struct_statfs=yes],
)
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
AC_TRY_COMPILE([
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
#  include <sys/mouht.h>
# elif STATFS_IN_SYS_STATFS
#  include <sys/statfs.h>
# endif
#endif ],
	[STATFS_t sfs;
	sfs.f_bavail = 0;],
	[AC_MSG_RESULT(yes)
	AC_DEFINE(STATFS_HAS_F_BAVAIL, 1, [Define if struct statfs has the f_bavail member])],
	[AC_MSG_RESULT(no)]
)
fi

if test X$found_struct_statfs = Xyes ; then
dnl
dnl now check to see if we have the 4-argument variant of statfs()
dnl this pretty much requires AC_TRY_RUN
dnl
AC_MSG_CHECKING([if statfs() requires 4 arguments])
AC_TRY_RUN([
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
],
	[AC_MSG_RESULT(yes)
	AC_DEFINE(STAT_STATFS4, 1, [Define if the statfs() call takes 4 arguments])],
	[AC_MSG_RESULT(no)],
	[AC_MSG_RESULT(no)]
)
fi

AC_C_INLINE

dnl look for libc features
PROVIDES_ERRNO=no
AC_MSG_CHECKING(if <netdb.h> defines h_errno)
AC_TRY_LINK([#include <netdb.h>],printf("%d",h_errno),PROVIDES_ERRNO=yes)
AC_MSG_RESULT($PROVIDES_ERRNO)
if test $PROVIDES_ERRNO = yes; then
	AC_DEFINE(HAVE_HERRNO, 1, [ Define as 1 if <netdb.h> defines h_errno])
fi

dnl If a system doesn't have S_IFSOCK, define it as 0 which will
dnl make S_ISSOCK always return false (nice, eh?)
AC_MSG_CHECKING(if <sys/stat.h> defines S_IFSOCK)
AC_TRY_LINK([#include <sys/stat.h>],printf("%d", S_IFSOCK),
	HAS_S_IFSOCK=yes,HAS_S_IFSOCK=no)
AC_MSG_RESULT($HAS_S_IFSOCK)
if test $HAS_S_IFSOCK = yes; then
	AC_DEFINE(HAVE_S_IFSOCK, 1, [Define as 1 if <sys/stat.h> defines S_IFSOCK])
fi

dnl Some Unix's are missing S_ISLNK, S_ISSOCK
AC_MSG_CHECKING(if <sys/stat.h> defines S_ISLNK)
AC_TRY_LINK([#include <sys/stat.h>],printf("%d", S_ISLNK(0755)),
	HAS_S_ISLNK=yes,HAS_S_ISLNK=no)
AC_MSG_RESULT($HAS_S_ISLNK)
if test $HAS_S_ISLNK = yes; then
	AC_DEFINE(HAVE_S_ISLNK, 1, [Define as 1 if <sys/stat.h> defines S_ISLNK])
fi

AC_MSG_CHECKING(if <sys/stat.h> defines S_ISSOCK)
AC_TRY_LINK([#include <sys/stat.h>],printf("%d", S_ISSOCK(0755)),
	HAS_S_ISSOCK=yes,HAS_S_ISSOCK=no)
AC_MSG_RESULT($HAS_S_ISSOCK)
if test $HAS_S_ISSOCK = yes; then
	AC_DEFINE(HAVE_S_ISSOCK, 1, [Define as 1 if <sys/stat.h> defines S_ISSOCK])
fi

AC_MSG_CHECKING(if timezone is defined)
AC_TRY_LINK([#include <time.h>],printf("%ld", timezone),
      HAS_TIMEZONE=yes,HAS_TIMEZONE=no)
AC_MSG_RESULT($HAS_TIMEZONE)

dnl Check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_PID_T

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_VPRINTF

dnl XXX AC_FUNC_MEMCMP
dnl XXX AC_FUNC_MMAP
dnl XXX AC_TYPE_SIGNAL
dnl XXX AC_FUNC_STRCOLL
dnl XXX AC_FUNC_STRFTIME
dnl XXX AC_FUNC_UTIME_NULL
dnl XXX AC_FUNC_VFORK
dnl XXX AC_CHECK_FUNCS(gethostname mkdir mkfifo rmdir select uname)

AC_CHECK_FUNCS(basename getcwd getwd inet_aton mtrace putenv realpath setenv)
AC_CHECK_FUNCS(stpcpy stpncpy strcspn)
AC_CHECK_FUNCS(strdup strerror strtol strtoul strspn strstr)

AC_CHECK_FUNCS(regcomp)

AC_CHECK_FUNCS(ftok)

dnl
dnl XXX Regex replacement isn't known to be needed yet.
dnl
dnl AC_ARG_WITH(regex, [  --with-regex            use the GNU regex library ],
dnl     [rpm_cv_regex=yes],
dnl     [AC_CHECK_FUNCS(regcomp, rpm_cv_regex=no, rpm_cv_regex=yes)])
dnl 
dnl if test $rpm_cv_regex = no ; then
dnl     AC_MSG_CHECKING(whether the regexp library is broken)
dnl     AC_TRY_RUN([
dnl #include <unistd.h>
dnl #include <regex.h>
dnl main() { regex_t blah ; exit(regcomp(&blah, "foo.*bar", REG_NOSUB) || regexec (&blah, "foobar", 0, NULL, 0)); }],
dnl     rpm_cv_regex_broken=no, rpm_cv_regex_broken=yes, rpm_cv_regex_broken=yes)
dnl     AC_MSG_RESULT([$rpm_cv_regex_broken])
dnl     if test $rpm_cv_regex_broken = yes ; then
dnl 	    echo "   --> using the included GNU regex instead." >&AC_FD_MSG
dnl 	    rpm_cv_regex=yes
dnl     fi
dnl fi
dnl 
dnl if test $rpm_cv_regex = yes; then
dnl     AC_DEFINE(USE_GNU_REGEX, 1)
dnl     AC_LIBOBJ(regex)
dnl fi

dnl
dnl XXX Glob *is* broken on linux with libc5, solaris and possibly aix when
dnl %files gets something like
dnl	/usr/*/locale/*/LC_MESSAGES/*.mo
dnl (Note: more than one asterisk in glob pattern.)
dnl
AC_ARG_WITH(glob, [  --with-glob             use the internal GNU glob ],
    [rpm_cv_glob=yes],
    [AC_CHECK_FUNCS(glob, rpm_cv_glob=no, rpm_cv_glob=yes)])

if test $rpm_cv_glob = no ; then
    AC_CACHE_CHECK([for GNU extensions to glob], rpm_cv_glob_ext, [
    rm -f t
    mkdir t
    mkdir t/p
    touch t/p/foo.8
    AC_TRY_RUN([
#include <unistd.h>
#include <sys/stat.h>
#include <glob.h>
main() {
  glob_t gl ;
  gl.gl_stat = stat;
  exit(glob("t/*/*.8", GLOB_PERIOD, NULL, &gl) || gl.gl_pathc < 1);
} ],
    rpm_cv_glob_ext=yes, rpm_cv_glob_ext=no, rpm_cv_glob_ext=no)
    rm -r t
    ])
    if test "$rpm_cv_glob_ext" = no ; then
	echo "    --> using the included GNU glob instead." >&AC_FD_MSG
	rpm_cv_glob=yes
    fi
fi

if test "$rpm_cv_glob" = yes; then
    AC_DEFINE(USE_GNU_GLOB, 1, [Use the included glob.c?])
    AC_LIBOBJ(glob)
    AC_LIBOBJ(fnmatch)
fi
dnl
dnl Auto-detect which python bindings should be built.
dnl
withval=auto
AC_ARG_WITH(python, [  --with-python           build rpm python bindings ])

WITH_PYTHON_VERSION=$withval
if test $withval = auto ; then

  AC_MSG_CHECKING(for python 2.2)
  AC_TRY_RUN([
#include <python2.2/Python.h>
main() {
  exit(strncmp("2.2", PY_VERSION, 3));
} ],
  withval=yes, withval=no, withval=yes)
  AC_MSG_RESULT($withval)
  if test $withval = yes ; then
    WITH_PYTHON_VERSION="2.2"
  else

    AC_MSG_CHECKING(for python 1.5.2)
    AC_TRY_RUN([
#include <python1.5/Python.h>
main() {
  exit(strcmp("1.5.2", PY_VERSION));
} ],
    withval=yes, withval=no, withval=yes)
    AC_MSG_RESULT($withval)
    if test $withval = yes ; then
      WITH_PYTHON_VERSION="1.5"
    fi
  fi

fi

if test "$WITH_PYTHON_VERSION" != no ; then
  WITH_PYTHON_SUBDIR=python
  WITH_PYTHON_SUBPACKAGE=1
else
  WITH_PYTHON_SUBDIR=
  WITH_PYTHON_SUBPACKAGE=0
fi
AC_SUBST(WITH_PYTHON_SUBDIR)
AC_SUBST(WITH_PYTHON_SUBPACKAGE)
AC_SUBST(WITH_PYTHON_VERSION)

AC_PATH_PROG(__DOXYGEN, doxygen, no, $PATH)
dnl
dnl Auto-detect whether doxygen generated API docs should be included.
dnl
withval=auto
AC_ARG_WITH(apidocs, [  --with-apidocs          build rpm API docs ])

if test $withval = auto -a $__DOXYGEN != no ; then
  withval=yes
elif test $withval = yes -a $__DOXYGEN = no ; then
  AC_MSG_ERROR(--> rpm API docs needs doxygen in PATH)
fi

if test $withval = yes; then
  WITH_APIDOCS_TARGET=apidocs
  WITH_APIDOCS=1
else
  WITH_APIDOCS=0
fi
AC_SUBST(WITH_APIDOCS_TARGET)
AC_SUBST(WITH_APIDOCS)

withval=no
AC_ARG_WITH(dmalloc, [  --with-dmalloc          build with dmalloc debugging support ])
if test $withval = yes ; then
  AC_DEFINE(DMALLOC, 1, [Build with dmalloc support?])
  LIBS="$LIBS -ldmalloc"
fi

withval=no
AC_ARG_WITH(efence, [  --with-efence           build with efence debugging support ])
if test $withval = yes ; then
  AC_DEFINE(EFENCE, 1, [Build with libefence support?])
  LIBS="$LIBS -lefence"
fi

AC_CHECK_FUNCS(setlocale)

dnl XXX Solaris <= 2.6 only permits 8 chars in password.
AC_CHECK_FUNCS(getpassphrase)

AC_CHECK_FUNC(getmntent, AC_DEFINE(HAVE_GETMNTENT, 1, [Define if you have the getmntent() function]), [
  AC_CHECK_FUNC(mntctl, AC_DEFINE(HAVE_MNTCTL, 1, [Define as 1 if you have mntctl() (only aix?)]),[
    AC_CHECK_FUNC(getmntinfo_r, AC_DEFINE(HAVE_GETMNTINFO_R, 1, [Define as 1 if you have getmntinfo_r() (only osf?)]), [
      AC_CHECK_LIB(c_r, getmntinfo_r, [LIBS="$LIBS -lc_r"; 
					AC_DEFINE(HAVE_GETMNTINFO_R, 1, [Define as 1 if you have getmntinfo_r() (only osf?)])], [
		 AC_DEFINE([USE_GETMNTENT], 1, [Defined if getmntent replacement is used])
                 AC_LIBOBJ(getmntent)])])])])

AC_CHECK_FUNC(lchown,
   [__CHOWN_RHF="%{__chown} -Rhf"
    __CHGRP_RHF="%{__chgrp} -Rhf"
    AC_DEFINE(HAVE_LCHOWN, 1, [Define as 1 if you have lchown()])],
   [__CHOWN_RHF="%{__chown} -Rf"
    __CHGRP_RHF="%{__chgrp} -Rf"
    dnl Does chown() follow symlinks? This should be a good enough test.
    AC_MSG_CHECKING(whether chown() follows symlinks)
    AC_ARG_ENABLE([broken-chown],
    [  --enable-broken-chown   this system's chown follows symbolic links], 
	    result=$enableval, result=unknown)
    if echo "$build" | egrep "(aix)|(hpux)|(linux)" > /dev/null ; then
	result=yes
    elif echo "$build" | egrep "(nextstep)" > /dev/null ; then
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

dnl
dnl figure out what root's primary group is
dnl
AC_MSG_CHECKING(root's primary group)
AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

int main()
{
	struct passwd *root = NULL;
	struct group *roots_group = NULL;
	FILE * tempfile = NULL;

	root = getpwuid( (uid_t) 0 );
	if (root != NULL) {
		roots_group = getgrgid(root->pw_gid);
		if (roots_group != NULL) {
			tempfile = fopen("conftest_rootg", "w");
			if (tempfile != NULL) {
				fprintf(tempfile, "%s\n", roots_group->gr_name);
				fclose(tempfile);
				exit(0);
			}
		}
	}

	exit(1);
}], ROOT_GROUP=`cat conftest_rootg`,
	ROOT_GROUP="root",ROOT_GROUP="root"
)
AC_MSG_RESULT($ROOT_GROUP)
AC_SUBST(ROOT_GROUP)

if test "x$varprefix" = "x"; then
    # For /usr and /usr/local, we want the 'var' directory to go
    # in /var and /var/local respectively. For everything else, 
    # just put the 'var' directory in prefix/var.
    case $prefix in
	/usr | /usr/local )
	    varprefix=`echo $prefix | sed 's/usr/var/'` ;;
	NONE)
 	    varprefix=`echo $ac_default_prefix | sed 's/usr/var/'` ;;
	*) 
	    varprefix=$prefix/var ;;
    esac	
fi
AC_SUBST(varprefix)

if test "x$tmpdir" = "x"; then
    if test -d $varprefix/tmp; then
	tmpdir=$varprefix/tmp
    else
	if test -d /var/tmp; then
	    tmpdir=/var/tmp
	else
	    tmpdir=/tmp
	fi
    fi
fi
AC_SUBST(tmpdir)

if echo "$build_os" | grep sco > /dev/null; then
	echo "hacking things up for sco"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(HAVE_STRUCT_MNTTAB, 1,
	[Define as 1 if you have "struct mnttab" (only sco?)])
elif echo "$build_os" | grep sunos > /dev/null; then
	echo "hacking things up for sunos"
	CFLAGS="$CFLAGS -D__USE_FIXED_PROTOTYPES__"
	AC_DEFINE(NEED_STRINGS_H, 1, [Define as one if we need to include <strings.h> (along with <string.h>)])
	AC_DEFINE(NEED_MYREALLOC, 1, [Define as 1 if we need myrealloc])
	AC_LIBOBJ(myrealloc)
fi

#
# get rid of the 4-th tuple, if config.guess returned "linux-gnu" for build_os
#
if echo "$build_os" | grep '.*-gnulibc1' > /dev/null ; then
	build_os=`echo "${build_os}" | sed 's/-gnulibc1$//'`
fi
if echo "$build_os" | grep '.*-gnu' > /dev/null ; then
	build_os=`echo "${build_os}" | sed 's/-gnu$//'`
fi

changequote(<, >)
build_os_exact="${build_os}"
build_os_major=`echo "${build_os}" | sed 's/\..*$//'`
build_os_noversion=`echo "${build_os}" | sed 's/[0-9]*\..*$//'`
changequote([, ])

rm -f ./find-provides
if test -f ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov ; then
	echo "using ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_exact}.prov ; then
	echo "using ${srcdir}/autodeps/${build_os_exact}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_os_exact}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_major}.prov ; then
	echo "using ${srcdir}/autodeps/${build_os_major}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_os_major}.prov ./find-provides
elif test -f ${srcdir}/autodeps/${build_os_noversion}.prov ; then
	echo "using ${srcdir}/autodeps/${build_os_noversion}.prov for automatic provides generation"
    ln -s ${srcdir}/autodeps/${build_os_noversion}.prov ./find-provides
else
    echo "*** no default provides information is available for ${build_os_noversion}"
    ln -s ${srcdir}/autodeps/none ./find-provides
fi

rm -f ./find-requires
if test -f ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req ; then
	echo "using ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_cpu}-${build_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_exact}.req ; then
	echo "using ${srcdir}/autodeps/${build_os_exact}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_os_exact}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_major}.req ; then
	echo "using ${srcdir}/autodeps/${build_os_major}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_os_major}.req ./find-requires
elif test -f ${srcdir}/autodeps/${build_os_noversion}.req ; then
	echo "using ${srcdir}/autodeps/${build_os_noversion}.req for automatic requires generation"
    ln -s ${srcdir}/autodeps/${build_os_noversion}.req ./find-requires
else
    echo "*** no default requires information is available for ${build_os_noversion}"
    ln -s ${srcdir}/autodeps/none ./find-requires
fi

dnl XXX Choose /usr/lib or /usr/lib64 for library installs.
MARK64=
case "${target_cpu}" in
x86_64*|ppc64*|powerpc64*|sparc64*|s390x*)	MARK64=64 ;;
esac
AC_SUBST(MARK64)

dnl Determine the canonical arch-vendor-os for the build machine
case "${build_cpu}" in
*86)		RPMCANONCOLOR=0; RPMCANONARCH=i386 ;;
x86_64*)	RPMCANONCOLOR=3; RPMCANONARCH=x86_64 ;;
alpha*)		RPMCANONCOLOR=0; RPMCANONARCH=alpha ;;
sparc*)		RPMCANONCOLOR=0; RPMCANONARCH=sparc ;;
ia64*)		RPMCANONCOLOR=0; RPMCANONARCH=ia64 ;;
s390x*)		RPMCANONCOLOR=0; RPMCANONARCH=s390x ;;
s390*)		RPMCANONCOLOR=0; RPMCANONARCH=s390 ;;
powerpc64*|ppc64*)	RPMCANONCOLOR=0; RPMCANONARCH=ppc64 ;;
powerpc*|ppc*)	RPMCANONCOLOR=0; RPMCANONARCH=ppc ;;
armv3l*)	RPMCANONCOLOR=0; RPMCANONARCH=armv3l ;;
armv4l*)	RPMCANONCOLOR=0; RPMCANONARCH=armv4l ;;
armv4b*)	RPMCANONCOLOR=0; RPMCANONARCH=armv4b ;;
arm*)		RPMCANONCOLOR=0; RPMCANONARCH="${build_cpu}" ;;
mipsel*)	RPMCANONCOLOR=0; RPMCANONARCH=mipsel ;;
mips*)		RPMCANONCOLOR=0; RPMCANONARCH=mips ;;
m68k*)		RPMCANONCOLOR=0; RPMCANONARCH=m68k ;;
*)		RPMCANONCOLOR=0; RPMCANONARCH=unknown ;;
esac
case "${build_os_noversion}" in
mint)		RPMCANONARCH=m68kmint ;;
esac
RPMCANONVENDOR="$build_vendor"
case "${build_vendor}" in
unknown|pc|ibm|redhat|pld|mandrake|conectiva|lvr|yellowdog)
	test -f /etc/redhat-release &&		RPMCANONVENDOR=redhat
	test -f /etc/pld-release &&		RPMCANONVENDOR=pld
	test -f /etc/mandrake-release &&	RPMCANONVENDOR=mandrake
	test -f /etc/conectiva-release &&	RPMCANONVENDOR=conectiva
	test -f /etc/lvr-release &&		RPMCANONVENDOR=lvr
	test -f /etc/yellowdog-release &&	RPMCANONVENDOR=yellowdog
	;;
esac
RPMCANONOS="$build_os_noversion"
AC_SUBST(RPMCANONCOLOR)
AC_SUBST(RPMCANONARCH)
AC_SUBST(RPMCANONVENDOR)
AC_SUBST(RPMCANONOS)

if test X"$prefix" = XNONE ; then
    usrprefix="$ac_default_prefix"
else
    usrprefix=$prefix
fi
LOCALEDIR="`echo ${usrprefix}/share/locale`"
AC_DEFINE_UNQUOTED(LOCALEDIR, "$LOCALEDIR",
	[Full path to rpm locale directory (usually /usr/share/locale)])
AC_SUBST(LOCALEDIR)
LIBDIR="`echo $libdir | sed 's-/lib$-/%{_lib}-'`"
AC_SUBST(LIBDIR)
RPMCONFIGDIR="`echo ${usrprefix}/lib/rpm`"
AC_DEFINE_UNQUOTED(RPMCONFIGDIR, "$RPMCONFIGDIR",
	[Full path to rpm global configuration directory (usually /usr/lib/rpm)])
AC_SUBST(RPMCONFIGDIR)

SYSCONFIGDIR="`echo /etc/rpm`"
AC_DEFINE_UNQUOTED(SYSCONFIGDIR, "$SYSCONFIGDIR",
	[Full path to rpm system configuration directory (usually /etc/rpm)])
AC_SUBST(SYSCONFIGDIR)

MACROFILES="${RPMCONFIGDIR}/macros:${RPMCONFIGDIR}/%{_target_platform}/macros:${SYSCONFIGDIR}/macros:${SYSCONFIGDIR}/macros.specspo:${SYSCONFIGDIR}/macros.db1:${SYSCONFIGDIR}/%{_target_platform}/macros:~/.rpmmacros"
AC_DEFINE_UNQUOTED(MACROFILES, "$MACROFILES",
	[Colon separated paths of macro files to read.])
AC_SUBST(MACROFILES)

LIBRPMRC_FILENAME="${RPMCONFIGDIR}/rpmrc"
AC_DEFINE_UNQUOTED(LIBRPMRC_FILENAME, "$LIBRPMRC_FILENAME",
	[Full path to rpmrc configuration file (usually /usr/lib/rpm/rpmrc)])
AC_SUBST(LIBRPMRC_FILENAME)

VENDORRPMRC_FILENAME="${RPMCONFIGDIR}/${RPMCANONVENDOR}/rpmrc"
AC_DEFINE_UNQUOTED(VENDORRPMRC_FILENAME, "$VENDORRPMRC_FILENAME",
       [Full path to vendor rpmrc configuration file (usually /usr/lib/rpm/vendor/rpmrc)])
AC_SUBST(VENDORRPMRC_FILENAME)

LIBRPMALIAS_FILENAME="${RPMCONFIGDIR}/rpmpopt-${VERSION}"
AC_DEFINE_UNQUOTED(LIBRPMALIAS_FILENAME, "$LIBRPMALIAS_FILENAME",
	[Full path to rpmpopt configuration file (usually /usr/lib/rpm/rpmpopt)])
AC_SUBST(LIBRPMALIAS_FILENAME)
FINDREQUIRES="${RPMCONFIGDIR}/find-requires"
AC_DEFINE_UNQUOTED(FINDREQUIRES, "$FINDREQUIRES",
	[Full path to find-requires script (usually /usr/lib/rpm/find-requires)])
AC_SUBST(FINDREQUIRES)
FINDPROVIDES="${RPMCONFIGDIR}/find-provides"
AC_DEFINE_UNQUOTED(FINDPROVIDES, "$FINDPROVIDES",
	[Full path to find-provides script (usually /usr/lib/rpm/find-provides)])
AC_SUBST(FINDPROVIDES)

testdir="`pwd`/tests"
dnl AC_DEFINE_UNQUOTED(testdir, "$testdir")
AC_SUBST(testdir)

AC_SUBST(INCPATH)
AC_SUBST(LIBMISC)

AC_SUBST(RPM)

AC_SUBST(OBJDUMP)

dnl XXX this causes popt to depend on zlib et al
dnl # XXX Propagate -lucb to popt ...
dnl export LIBS INCPATH CONFIG_SITE

AC_CONFIG_SUBDIRS(popt beecrypt zlib elfutils file db3)

AC_OUTPUT([ Doxyfile Makefile rpmrc macros platform rpmpopt rpm.spec
	rpmio/Makefile rpmdb/Makefile lib/Makefile build/Makefile
	scripts/Makefile scripts/brp-redhat
	scripts/macros.perl scripts/macros.php scripts/macros.python
	tools/Makefile
	tests/Makefile tests/rpmrc tests/macros tests/hello-test/Makefile
	misc/Makefile intl/Makefile po/Makefile.in
	doc/Makefile
	doc/manual/Makefile
	doc/fr/Makefile
	doc/ja/Makefile
	doc/ko/Makefile
	doc/pl/Makefile
	doc/ru/Makefile
	doc/sk/Makefile
	python/Makefile
	python/rpmdb/Makefile
	python/test/Makefile
  ], [	echo timestamp > popt/stamp-h.in
	echo timestamp > beecrypt/stamp-h.in
	echo timestamp > stamp-h.in
  ]
)
