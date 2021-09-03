AC_PREREQ(2.61)
AC_INIT(rpm, 4.17.0, rpm-maint@lists.rpm.org)

AC_CONFIG_SRCDIR([rpm.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10 foreign tar-pax dist-bzip2 subdir-objects nostdinc])
dnl Allow silent build on automake versions that support it
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

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

AC_SUBST(__CC, $CC)
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" = no; then
    AC_MSG_ERROR([ISO C99 capable compiler required])
fi

AS=${AS-as}
AC_SUBST(AS)
if test "$GCC" = yes; then
    cflags_to_try="-fno-strict-aliasing -fno-strict-overflow -fno-delete-null-pointer-checks -Wempty-body"
    AC_MSG_CHECKING([supported compiler flags])
    old_cflags=$CFLAGS
    echo
    for flag in $cflags_to_try; do
        CFLAGS="$CFLAGS $flag -Werror"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[#include <alloca.h>
						alloca(100);]])],[
                echo "   $flag"
                RPMCFLAGS="$RPMCFLAGS $flag"
        ],[])
        CFLAGS=$old_cflags
    done
    RPMCFLAGS="-Wall -Wpointer-arith -Wmissing-prototypes -Wstrict-prototypes $RPMCFLAGS"
fi
AC_ARG_ENABLE(werror, [AS_HELP_STRING([--enable-werror], [build with -Werror])],
		[RPMCFLAGS="$RPMCFLAGS -Werror"], [])
AC_SUBST(RPMCFLAGS)

AC_SYS_LARGEFILE

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
AC_PATH_PROGS(__7ZIP, [7zip 7za 7z], /usr/bin/7za, $MYPATH)
AC_PATH_PROG(__BZIP2, bzip2, /usr/bin/bzip2, $MYPATH)
AC_PATH_PROG(__CAT, cat, /bin/cat, $MYPATH)
AC_PATH_PROG(__CHMOD, chmod, /bin/chmod, $MYPATH)
AC_PATH_PROG(__CHOWN, chown, /bin/chown, $MYPATH)
AC_PATH_PROG(__CP, cp, /bin/cp, $MYPATH)
AC_PATH_PROG(__CURL, curl, /usr/bin/curl, $MYPATH)
AC_PATH_PROG(__FILE, file, /usr/bin/file, $MYPATH)
AC_PATH_PROGS(__GPG, gpg2 gpg, /usr/bin/gpg2, $MYPATH)
AC_PATH_PROG(__GREP, grep, /bin/grep, $MYPATH)
AC_PATH_PROG(__GZIP, gzip, /bin/gzip, $MYPATH)
AC_PATH_PROG(__UNZIP, unzip, /usr/bin/unzip, $MYPATH)
AC_PATH_PROG(__ID, id, /usr/bin/id, $MYPATH)
AC_PATH_PROG(__INSTALL, install, /usr/bin/install, $MYPATH)
AC_PATH_PROG(__LRZIP, lrzip, /usr/bin/lrzip, $MYPATH)
AC_PATH_PROG(__LZIP, lzip, /usr/bin/lzip, $MYPATH)
AC_PATH_PROG(__XZ, xz, /usr/bin/xz, $MYPATH)
AC_PATH_PROG(__GEM, gem, /usr/bin/gem, $MYPATH)
AC_PATH_PROG(__MAKE, make, /usr/bin/make, $MYPATH)
AC_PATH_PROG(__MKDIR, mkdir, /bin/mkdir, $MYPATH)
AC_PATH_PROG(__MV, mv, /bin/mv, $MYPATH)
AC_PATH_PROG(__PATCH, patch, /usr/bin/patch, $MYPATH)
AC_PATH_PROG(__RM, rm, /bin/rm, $MYPATH)
AC_PATH_PROG(__SED, sed, /bin/sed, $MYPATH)
AC_PATH_PROG(__TAR, tar, /bin/tar, $MYPATH)
AC_PATH_PROG(__ZSTD, zstd, /usr/bin/zstd, $MYPATH)

AC_PATH_PROG(__LD, ld, /usr/bin/ld, $MYPATH)
AC_PATH_PROG(__OBJDUMP, objdump, /usr/bin/objdump, $MYPATH)
AC_PATH_PROG(__STRIP, strip, /usr/bin/strip, $MYPATH)
AC_PATH_PROG(__FIND_DEBUGINFO, find-debuginfo, /usr/bin/find-debuginfo, $MYPATH)

AC_PATH_PROG(__GIT, git, /usr/bin/git, $MYPATH)
AC_PATH_PROG(__HG, hg, /usr/bin/hg, $MYPATH)
AC_PATH_PROG(__BZR, bzr, /usr/bin/bzr, $MYPATH)
AC_PATH_PROG(__QUILT, quilt, /usr/bin/quilt, $MYPATH)

AC_PATH_PROG(__FAKECHROOT, fakechroot, no, $MYPATH)
AM_CONDITIONAL(HAVE_FAKECHROOT, [test "$__FAKECHROOT" != "no"])

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
  AC_CHECK_LIB(bz2, BZ2_bzread, [WITH_BZ2_LIB=-lbz2])
])
AC_SUBST(WITH_BZ2_LIB)

#=================
# Check for lzma library.

AC_CHECK_HEADERS([lzma.h],[
  AC_CHECK_LIB(lzma, lzma_easy_encoder, [WITH_LZMA_LIB=-llzma])
])
AC_SUBST(WITH_LZMA_LIB)

# AC_OPENMP supports --enable/disable-openmp out of the box, but it doesn't
# actually give us a way to conditionalize the build based on that. Argh.
# Version 4.5 (201511) introduced "priority" clause for tasks.
OPENMP_CFLAGS=
AC_OPENMP
AS_IF([test "x$ac_cv_prog_c_openmp" != x &&
       test "x$ac_cv_prog_c_openmp" != xunsupported],[
  old_CFLAGS=$CFLAGS
  CFLAGS="$CFLAGS $OPENMP_CFLAGS"
  AC_MSG_CHECKING([OpenMP is at least version 4.5])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [#include <omp.h>],
      [#if _OPENMP < 201511
       #error
       #endif
      ]
    )],
    [AC_MSG_RESULT([yes])
     AC_DEFINE(ENABLE_OPENMP, 1, [Enable multithreading support?])
    ],
    [AC_MSG_RESULT([no])
     OPENMP_CFLAGS=
     if test "$enable_openmp" = "yes"; then
       AC_MSG_ERROR([OpenMP too old])
     fi
    ]
  )
  CFLAGS=$old_CFLAGS
])
AC_SUBST(OPENMP_CFLAGS)

#=================
# Check for zstd.

AC_ARG_ENABLE([zstd],
              [AS_HELP_STRING([--enable-zstd=@<:@yes/no/auto@:>@],
                              [build without zstd support (default=auto)])],
              [enable_zstd="$enableval"],
              [enable_zstd=auto])

AS_IF([test "x$enable_zstd" != "xno"], [
  PKG_CHECK_MODULES([ZSTD], [libzstd >= 1.3.8], [have_zstd=yes], [have_zstd=no])
  AS_IF([test "$enable_zstd" = "yes"], [
    if test "$have_zstd" = "no"; then
      AC_MSG_ERROR([--enable-zstd specified, but not available])
    fi
  ])
])

if test "x$have_zstd" = "xyes"; then
  AC_DEFINE([HAVE_ZSTD], [1], [Define if libzstd is available])
  ZSTD_REQUIRES=libzstd
  AC_SUBST(ZSTD_REQUIRES)
fi
AM_CONDITIONAL([HAVE_ZSTD], [test "x$have_zstd" = "xyes"])

#=================

dnl
dnl Check for features
dnl

dnl Checks for libraries.

AC_CHECK_FUNC(gethostname, [], [
    AC_CHECK_LIB(nsl, gethostname)
])

AC_CHECK_HEADERS(pthread.h)

AC_CHECK_LIB(pthread, pthread_mutex_trylock, [], [
  dnl OSF 5.0 has the the symbols prefixed with __ in libpthread.
  AC_CHECK_LIB(pthread, __pthread_mutex_trylock, [], [
    AC_CHECK_LIB(thread, mutex_lock)
  ])
])

AC_SEARCH_LIBS(dlopen, [dl])

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

#=================
# Select crypto library
AC_ARG_WITH(crypto,
            [AC_HELP_STRING([--with-crypto=CRYPTO_LIB],
                            [The cryptographic library to use (openssl|libgcrypt). The default is libgcrypt.])
                            ],[],
                            [with_crypto=libgcrypt])

#=================
# Check for OpenSSL library.
# We need evp.h from OpenSSL.

WITH_OPENSSL_INCLUDE=
WITH_OPENSSL_LIB=
if test "$with_crypto" = openssl; then
# If we have pkgconfig make sure CPPFLAGS are setup correctly for the OpenSSL
# -I include path.
AC_PATH_TOOL([PKGCONFIG], [pkg-config], [no], [$PATH:/usr/bin:/usr/local/bin])
if test "x$PKGCONFIG" != "xno"; then
  CPPFLAGS="$CPPFLAGS $($PKGCONFIG --cflags libcrypto)"
  WITH_OPENSSL_LIB=$($PKGCONFIG --libs libcrypto)
else
  WITH_OPENSSL_LIB=-lcrypto
fi

AC_CHECK_HEADERS([openssl/evp.h], [], [
  AC_MSG_ERROR([missing required OpenSSL header])
])
AC_CHECK_HEADERS([openssl/rsa.h], [], [
  AC_MSG_ERROR([missing required OpenSSL header])
])
AC_CHECK_HEADERS([openssl/dsa.h], [], [
  AC_MSG_ERROR([missing required OpenSSL header])
])

AC_CHECK_LIB(crypto, EVP_DigestInit_ex, [], [
  AC_MSG_ERROR([required OpenSSL library 'libcrypto' missing or too old])
])

AC_CHECK_LIB(crypto, EVP_MD_CTX_new, [
    AC_DEFINE(HAVE_EVP_MD_CTX_NEW, 1, [Define to 1 if OpenSSL has EVP_MD_CTX_new])
    AC_SUBST(HAVE_EVP_MD_CTX_NEW, [1])
  ], [
  AC_CHECK_LIB(crypto, EVP_MD_CTX_create, [], [
      AC_MSG_ERROR([required OpenSSL library 'libcrypto' missing or too old])
  ])
])

AC_CHECK_LIB(crypto, EVP_PKEY_CTX_new, [], [
  AC_MSG_ERROR([required OpenSSL library 'libcrypto' missing or too old])
])

AC_CHECK_LIB(crypto, DSA_set0_key, [
    AC_DEFINE(HAVE_DSA_SET0_KEY, 1, [Define to 1 if OpenSSL has DSA_set0_key])
    AC_SUBST(HAVE_DSA_SET0_KEY, [1])
    ], []
)

AC_CHECK_LIB(crypto, DSA_set0_pqg, [
    AC_DEFINE(HAVE_DSA_SET0_PQG, 1, [Define to 1 if OpenSSL has DSA_set0_pqg])
    AC_SUBST(HAVE_DSA_SET0_PQG, [1])
    ], []
)

AC_CHECK_LIB(crypto, DSA_SIG_set0, [
    AC_DEFINE(HAVE_DSA_SIG_SET0, 1, [Define to 1 if OpenSSL has DSA_SIG_set0])
    AC_SUBST(HAVE_DSA_SIG_SET0, [1])
    ], []
)

AC_CHECK_LIB(crypto, RSA_set0_key, [
    AC_DEFINE(HAVE_RSA_SET0_KEY, 1, [Define to 1 if OpenSSL has RSA_set0_key])
    AC_SUBST(HAVE_RSA_SET0_KEY, [1])
    ], []
)

AC_CHECK_LIB(crypto, BN_bn2binpad, [
    AC_DEFINE(HAVE_BN2BINPAD, 1, [Define to 1 if OpenSSL has BN_bn2binpad])
    AC_SUBST(HAVE_BN2BINPAD, [1])
    ], []
)

fi

AM_CONDITIONAL([WITH_OPENSSL],[test "$with_crypto" = openssl])
AC_SUBST(WITH_OPENSSL_INCLUDE)
AC_SUBST(WITH_OPENSSL_LIB)

#=================
# Check for libgcrypt library.
WITH_LIBGCRYPT_INCLUDE=
WITH_LIBGCRYPT_LIB=
if test "$with_crypto" = libgcrypt ; then
  # libgcrypt 1.8.5 onwards ships a pkg-config file so prefer that
  PKG_CHECK_MODULES([LIBGCRYPT], [libgcrypt], [have_libgcrypt=yes], [have_libgcrypt=no])
  if test "$have_libgcrypt" = "yes"; then
    WITH_LIBGCRYPT_INCLUDE="$LIBGCRYPT_CFLAGS"
    WITH_LIBGCRYPT_LIB="$LIBGCRYPT_LIBS"
  else
    AC_PATH_TOOL(LIBGCRYPT_CONFIG, libgcrypt-config, notfound)
      if test notfound != "$LIBGCRYPT_CONFIG" ; then
        WITH_LIBGCRYPT_INCLUDE=`$LIBGCRYPT_CONFIG --cflags`
        WITH_LIBGCRYPT_LIB=`$LIBGCRYPT_CONFIG --libs`
     fi
     if test -z "$WITH_LIBGCRYPT_LIB" ; then
       AC_MSG_ERROR([libgcrypt not found])
    fi
  fi
fi

AM_CONDITIONAL([WITH_LIBGCRYPT],[test "$with_crypto" = libgcrypt])
AC_SUBST(WITH_LIBGCRYPT_INCLUDE)
AC_SUBST(WITH_LIBGCRYPT_LIB)

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
# Check for libarchive library.
AC_ARG_WITH([archive], [AS_HELP_STRING([--with-archive], [build rpm2archive - requires libarchive])],
            [],
            [with_archive=yes])

WITH_ARCHIVE_INCLUDE=
WITH_ARCHIVE_LIB=
AS_IF([test "$with_archive" != no],[
  AC_CHECK_HEADER([archive.h], [
    AC_CHECK_LIB(archive, archive_write_open_filename, [
      WITH_ARCHIVE_INCLUDE=
      WITH_ARCHIVE_LIB="-larchive"
    ],[
      AC_MSG_ERROR([missing required library 'libarchive'])
    ])
],[
      AC_MSG_ERROR([missing required header archive.h])
  ])
])

AC_SUBST(WITH_ARCHIVE_INCLUDE)
AC_SUBST(WITH_ARCHIVE_LIB)
AM_CONDITIONAL(WITH_ARCHIVE,[test "$with_archive" = yes])

#=================
# Check for elfutils libdw library with dwelf_elf_gnu_build_id.
WITH_LIBDW_LIB=
AS_IF([test "$WITH_LIBELF" = yes],[
  AC_CHECK_HEADERS([elfutils/libdwelf.h],[
    # dwelf_elf_gnu_build_id was introduced in elfutils 0.159
    AC_CHECK_LIB(dw, dwelf_elf_gnu_build_id, [
      AC_DEFINE(HAVE_LIBDW, 1,
                [Define to 1 if you have elfutils libdw library])
      WITH_LIBDW_LIB="-ldw"
      WITH_LIBDW=yes
      # whether libdw supports compressed ELF objects
      AC_CHECK_LIB(dw, dwelf_elf_begin, [
                   AC_DEFINE(HAVE_DWELF_ELF_BEGIN, 1, [Have dwelf_elf_begin?])
      ])
    ])
  ])
])
AC_SUBST(WITH_LIBDW_LIB)
AM_CONDITIONAL(LIBDW,[test "$WITH_LIBDW" = yes])

#=================
# Process --enable-ndb
AC_ARG_ENABLE([ndb], [AS_HELP_STRING([--enable-ndb],[enable the new rpm database format])],
[case "$enable_ndb" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --enable-ndb])
  ;;
esac],
[enable_ndb=no])
AS_IF([test "$enable_ndb" = yes],[
  AC_DEFINE(ENABLE_NDB, 1, [Enable new rpm database format?])
  AC_CHECK_FUNCS(mremap, [], [], [#include <sys/mman.h>])
])
AM_CONDITIONAL([NDB], [test "$enable_ndb" = yes])

#=================
# Process --enable-bdb-ro
AC_ARG_ENABLE([bdb-ro], [AS_HELP_STRING([--enable-bdb-ro (EXPERIMENTAL)],[enable the read-only Berkeley DB code])],
[case "$enable_bdb_ro" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --enable-bdb-ro])
  ;;
esac],
[enable_bdb_ro=no])
AS_IF([test "$enable_bdb_ro" = yes],[
  AC_DEFINE(WITH_BDB_RO, 1, [Build with read-only Berkeley DB])
])
AM_CONDITIONAL([BDB_RO], [test "$enable_bdb_ro" = yes])

# Check for SQLITE support
AC_ARG_ENABLE([sqlite],
              [AS_HELP_STRING([--enable-sqlite=@<:@yes/no/auto@:>@)],
                              [build with sqlite rpm database format support (default=yes)])],
              [enable_sqlite="$enableval"],
              [enable_sqlite=yes])

AS_IF([test "x$enable_sqlite" != "xno"], [
  PKG_CHECK_MODULES([SQLITE], [sqlite3 >= 3.22.0], [have_sqlite=yes], [have_sqlite=no])
  AS_IF([test "$enable_sqlite" = "yes"], [
    if test "$have_sqlite" = "no"; then
      AC_MSG_ERROR([--enable-sqlite specified, but not available])
    fi
  ])
])

if test "x$have_sqlite" = "xyes"; then
  AC_DEFINE([WITH_SQLITE], [1], [Define if SQLITE is available])
  SQLITE_REQUIRES=sqlite
  AC_SUBST(SQLITE_REQUIRES)
fi
AM_CONDITIONAL([SQLITE], [test "x$have_sqlite" = "xyes"])

AM_GNU_GETTEXT_VERSION([0.18.2])
AM_GNU_GETTEXT([external])
AM_ICONV

dnl Checks for header files we can live without.
AC_HEADER_STDC
dnl glibc and autoconf don't really play well together.
dnl glibc will produce a warning when including the wrong header.
dnl but still define major and minor. Causing us to include the header
dnl that produces a giant warning for each major/minor use.
dnl Use -Werror to work around that.
old_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Werror"
AC_HEADER_MAJOR
CFLAGS=$old_CFLAGS
AC_STRUCT_DIRENT_D_TYPE

AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(getopt.h)

AC_CHECK_HEADERS(sys/utsname.h)

AC_CHECK_HEADERS(sys/systemcfg.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(sys/auxv.h)

dnl look for libc features

dnl Check for missing typedefs
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(putenv)
AC_CHECK_FUNCS(mempcpy)
AC_CHECK_FUNCS(fdatasync)
AC_CHECK_DECLS(fdatasync, [], [], [#include <unistd.h>])
AC_CHECK_FUNCS(lutimes)
AC_CHECK_FUNCS(mergesort)
AC_CHECK_FUNCS(getauxval)
AC_CHECK_FUNCS(setprogname, [], [], [#include <stdlib.h>])
AC_CHECK_FUNCS(syncfs)
AC_CHECK_FUNCS(sched_getaffinity, [], [], [#include <sched.h>])
AC_CHECK_FUNCS(unshare, [], [], [#include <sched.h>])

AC_MSG_CHECKING([whether __progname is defined])
AC_LINK_IFELSE([AC_LANG_PROGRAM([extern const char *__progname;],
	   [if (*__progname == 0) return 0;])],
	   AC_DEFINE([HAVE___PROGNAME], [1], [Define if __progname is defined])
	   AC_MSG_RESULT([yes]),
	   AC_MSG_RESULT([no]))

AC_REPLACE_FUNCS(stpcpy stpncpy)

AC_CHECK_FUNCS([secure_getenv __secure_getenv])

AC_CHECK_FUNCS(
   [mkstemp getcwd basename dirname realpath setenv unsetenv regcomp lchown \
    utimes getline localtime_r statvfs getaddrinfo ],
   [], [AC_MSG_ERROR([function required by rpm])])

AC_LIBOBJ(fnmatch)

dnl check if python is requested
AC_ARG_ENABLE(python, [AS_HELP_STRING([--enable-python],[build rpm python bindings])],
[case "$enable_python" in
yes|no) ;;
*) AC_MSG_ERROR([invalid argument to --enable-python])
  ;;
esac],
[enable_python=no])

WITH_PYTHON_SUBPACKAGE=0
AS_IF([test "$enable_python" = yes],[
  PYTHON="${PYTHON:-python3}"
  AM_PATH_PYTHON([3.1],[
    PKG_CHECK_MODULES([PYTHON], [python-${PYTHON_VERSION}], [WITH_PYTHON_SUBPACKAGE=1])
    AC_SUBST(PYTHON_CFLAGS)
    AC_SUBST(PYTHON_LIB)
  ])
])

AM_CONDITIONAL(ENABLE_PYTHON,[test "$WITH_PYTHON_SUBPACKAGE" = 1])

AC_PATH_PROG(DOXYGEN, doxygen, no)
AC_CHECK_PROG(HAVE_DOT,[dot],[YES],[NO])
AC_PATH_PROG(PANDOC, pandoc, no)

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
    AC_CHECK_LIB([selinux],[setexecfilecon],[
      AC_DEFINE([HAVE_SETEXECFILECON],[1],[Define to 1 if SELinux setexecfilecon is present])])
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
])

AS_IF([test "$with_selinux" = yes],[
  AC_DEFINE(WITH_SELINUX, 1, [Build with selinux support?])
  WITH_SELINUX_LIB="-lselinux"
])
AC_SUBST(WITH_SELINUX_LIB)
AC_SUBST(WITH_SEMANAGE_LIB)
AM_CONDITIONAL(SELINUX,[test "$with_selinux" = yes])

# libimaevm
with_iamevm=no
AC_ARG_WITH(imaevm, [AS_HELP_STRING([--with-imaevm],[build with imaevm support])])
if test "$with_imaevm" = yes ; then
  AC_MSG_CHECKING([libimaevm >= 1.0])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <imaevm.h>]],
      [[return sign_hash(NULL, NULL, 0, NULL, NULL, NULL);]]
    )],
    [AC_MSG_RESULT(yes)
     AC_DEFINE(WITH_IMAEVM, 1, [Build with imaevm support?])
     WITH_IMAEVM_LIB="-limaevm"
    ],
    [AC_MSG_ERROR([libimaevm not present or too old (< 1.0)])]
  )
fi
AM_CONDITIONAL(WITH_IMAEVM,[test "$with_imaevm" = yes])
AC_SUBST(WITH_IMAEVM_LIB)

# fsverity
AC_ARG_WITH([fsverity], [AS_HELP_STRING([--with-fsverity],[build with fsverity support])],[],[with_fsverity=no])
if test "$with_fsverity" = yes ; then
  AC_MSG_CHECKING([libfsverity])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <libfsverity.h>]],
      [[return libfsverity_sign_digest(NULL, NULL, NULL, NULL);]]
    )],
    [AC_MSG_RESULT(yes)
      AC_DEFINE(WITH_FSVERITY, 1, [Build with fsverity support?])
      WITH_FSVERITY_LIB="-lfsverity"
    ],
    [AC_MSG_ERROR([--with-fsverity given, libfsverity or libfsverity.h missing])]
  )
fi
AM_CONDITIONAL(WITH_FSVERITY,[test "$with_fsverity" = yes])
AC_SUBST(WITH_FSVERITY_LIB)

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

PKG_CHECK_MODULES([LUA],
    [lua >= 5.3],
    [],
    [AC_MSG_ERROR([lua not present or too old)])])
AC_SUBST(LUA_CFLAGS)
AC_SUBST(LUA_LIBS)

AC_ARG_ENABLE(plugins, [AS_HELP_STRING([--disable-plugins],[build without plugin support])],,[enable_plugins=yes])
AS_IF([test "$enable_plugins" = yes],[
  AC_DEFINE(ENABLE_PLUGINS, 1, [Build with plugin support?])
])
AM_CONDITIONAL(ENABLE_PLUGINS,[test "$enable_plugins" = yes])

AC_ARG_ENABLE(inhibit_plugin, [AS_HELP_STRING([--disable-inhibit-plugin],[build without systemd inhibit plugin])],,[enable_inhibit_plugin=yes])
AS_IF([test "$enable_inhibit_plugin" = yes],[
  AC_DEFINE(ENABLE_INHIBIT_PLUGIN, 1, [Build with systemd inhibit plugin?])
])
AM_CONDITIONAL(ENABLE_INHIBIT_PLUGIN,[test "$enable_inhibit_plugin" = yes])

with_dbus=no
AS_IF([test "$enable_plugins" != no],[
 AS_IF([test "$enable_inhibit_plugin" != no],[
  PKG_CHECK_MODULES([DBUS],
    [dbus-1 >= 1.3],
    [AC_DEFINE(DBUS, 1, [Build with dbus support?]) with_dbus=yes],
    [with_dbus=no])
  AC_SUBST(DBUS_CFLAGS)
  AC_SUBST(DBUS_LIBS)
 ])
])
AM_CONDITIONAL(DBUS, [test "$with_dbus" = yes])

AS_IF([test "$enable_plugins" != no],[
  AC_CHECK_FUNCS(lsetxattr, [], [], [#include <sys/xattr.h>])
])
AM_CONDITIONAL(IMA, [test "x$ac_cv_func_lsetxattr" = xyes])

AS_IF([test "$enable_plugins" != no],[
AC_CHECK_HEADERS([linux/fsverity.h],[FSVERITY_IOCTL="yes"])
])
AM_CONDITIONAL(FSVERITY_IOCTL,[test "x$FSVERITY_IOCTL" = xyes])

#=================
# Check for fapolicyd support
AC_ARG_WITH(fapolicyd,
AS_HELP_STRING([--with-fapolicyd],[build with File Access Policy Daemon support]),
with_fapolicyd=$withval,
with_fapolicyd=auto)
AM_CONDITIONAL(FAPOLICYD,[test "$with_fapolicyd" = yes])

#=================
# Check for audit library.
AC_ARG_WITH(audit,
AS_HELP_STRING([--with-audit],[Linux audit plugin]),
with_audit=$withval,
with_audit=auto)

WITH_AUDIT_LIB=
AS_IF([test "$enable_plugins" != no],[
  AS_IF([test "x$with_audit" != xno],[
    AC_SEARCH_LIBS([audit_open],[audit],[
      WITH_AUDIT_LIB="$ac_res"
      AC_DEFINE(WITH_AUDIT, 1, [libaudit support])
      with_audit=yes
    ],[
      if test "x$with_audit" != xauto; then
        AC_MSG_ERROR([missing audit library])
      fi
    ])
  ])
])
AC_SUBST(WITH_AUDIT_LIB)
AM_CONDITIONAL(AUDIT,[test "$with_audit" = yes])

user_with_uid0=$(awk -F: '$3==0 {print $1;exit}' /etc/passwd)
group_with_gid0=$(awk -F: '$3==0 {print $1;exit}' /etc/group)
AC_DEFINE_UNQUOTED([UID_0_USER],["$user_with_uid0"],[Get the user name having userid 0])
AC_DEFINE_UNQUOTED([GID_0_GROUP],["$group_with_gid0"],[Get the group name having groupid 0])

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

dnl permit overriding build_vendor for "canonical rpm vendor"
RPMCANONVENDOR="$build_vendor"
AC_ARG_WITH([vendor], 
  AS_HELP_STRING([--with-vendor=VENDOR], [specify rpm vendor name]),
  [RPMCANONVENDOR=$withval])
AC_DEFINE_UNQUOTED([RPMCANONVENDOR],["${RPMCANONVENDOR}"],[canonical vendor])
	   
RPMCANONOS="$host_os_noversion"
RPMCANONGNU="$host_os_gnu"
AC_SUBST(RPMCANONVENDOR)
AC_SUBST(RPMCANONOS)
AC_SUBST(RPMCANONGNU)

RUNDIR="/run"
AC_ARG_WITH([rundir], 
  AS_HELP_STRING([--with-rundir=RUNDIR], [specify run-time variable directory]),
  [RUNDIR=$withval])
AC_DEFINE_UNQUOTED([RUNDIR],["${RUNDIR}"],[run-time variable directory])
AC_SUBST(RUNDIR)

if test X"$prefix" = XNONE ; then
    usrprefix="$ac_default_prefix"
else
    usrprefix=$prefix
fi

RPMCONFIGDIR="`echo ${usrprefix}/lib/rpm`"
AC_SUBST(RPMCONFIGDIR)

AC_SUBST(OBJDUMP)

AM_CONDITIONAL([DOXYGEN],[test "$DOXYGEN" != no])
AM_CONDITIONAL([PANDOC],[test "$PANDOC" != no])

AC_PATH_PROG(AUTOM4TE,autom4te,:)

AC_SUBST([dirstamp],[\${am__leading_dot}dirstamp])

AC_CONFIG_FILES([Makefile
	rpmio/Makefile lib/Makefile build/Makefile sign/Makefile
	po/Makefile.in scripts/Makefile fileattrs/Makefile
	misc/Makefile 
	docs/Makefile
	docs/man/Makefile
	python/Makefile
 	luaext/Makefile
 	tests/Makefile
 	plugins/Makefile
	python/setup.py
  ])
AC_OUTPUT
