dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([file],[4.10])
AC_CANONICAL_TARGET
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE

AC_MSG_CHECKING(for builtin ELF support)
AC_ARG_ENABLE(elf,
[  --disable-elf            disable builtin ELF support],
AH_TEMPLATE([BUILTIN_ELF],     [Define if builtin ELF support is enabled.])
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([BUILTIN_ELF], 1)
else
  AC_MSG_RESULT(no)
fi], [
  # enable by default
  AC_MSG_RESULT(yes)
  AC_DEFINE([BUILTIN_ELF], 1)
])

AC_MSG_CHECKING(for ELF core file support)
AC_ARG_ENABLE(elf-core,
[  --disable-elf-core       disable ELF core file support],
AH_TEMPLATE([ELFCORE], [Define if ELF core file support is enabled.])
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([ELFCORE], 1)
else
  AC_MSG_RESULT(no)
fi], [
  # enable by default
  AC_MSG_RESULT(yes)
  AC_DEFINE([ELFCORE], 1)
])

AC_MSG_CHECKING(for file formats in man section 5)
AC_ARG_ENABLE(fsect-man5,
[  --enable-fsect-man5      enable file formats in man section 5],
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  fsect=5
else
  AC_MSG_RESULT(no)
  fsect=4
fi], [
  # disable by default
  AC_MSG_RESULT(no)
  fsect=4
])
AC_SUBST(fsect)
AM_CONDITIONAL(FSECT5, test x$fsect = x5)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

if test "$ac_cv_c_compiler_gnu" = yes; then
    CFLAGS="$CFLAGS -D_GNU_SOURCE -D_REENTRANT -Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts"
fi
export CFLAGS

dnl Checks for headers
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_SYS_WAIT
AC_HEADER_STDINT
AC_CHECK_HEADERS(fcntl.h locale.h)
AC_CHECK_HEADERS(sys/mman.h sys/stat.h sys/types.h)
AC_CHECK_HEADERS(stdint.h inttypes.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(getopt.h)
AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(utime.h)
AC_CHECK_HEADERS(sys/utime.h)
AC_CHECK_HEADERS(regex.h)
AC_CHECK_HEADERS(wchar.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_RDEV
AH_TEMPLATE([HAVE_DAYLIGHT], [Define if we have a global "int" variable "daylight".])
AH_TEMPLATE([HAVE_TM_ISDST], [Define if we have "tm_isdst" in "struct tm".])
AC_STRUCT_TIMEZONE_DAYLIGHT
AC_SYS_LARGEFILE
AC_MBSTATE_T

AH_TEMPLATE([uint8_t], [Define to `unsigned char' if standard headers don't define.])
AH_TEMPLATE([uint16_t], [Define to `unsigned short' if standard headers don't define.])
AH_TEMPLATE([uint32_t], [Define to `unsigned int' if standard headers don't define.])
AH_TEMPLATE([int32_t], [Define to `int' if standard headers don't define.])
AH_TEMPLATE([HAVE_LONG_LONG],[Define if the `long long' type works.])
AH_TEMPLATE([uint64_t], [Define to `unsigned long long', if available, or `unsigned long', if standard headers don't define.])
AC_CHECK_TYPE_STDC(uint8_t, unsigned char)
AC_CHECK_TYPE_STDC(uint16_t, unsigned short)
AC_CHECK_TYPE_STDC(uint32_t, unsigned int)
AC_CHECK_TYPE_STDC(int32_t, int)
AC_C_LONG_LONG
if test $ac_cv_c_long_long = yes; then
  long64='unsigned long long';
else
  long64='unsigned long';
fi
dnl This needs a patch to autoconf 2.13 acgeneral.m4
AC_CHECK_TYPE2_STDC(uint64_t, $long64)

AH_TEMPLATE([SIZEOF_UINT8_T], [The number of bytes in a uint8_t.])
AH_TEMPLATE([SIZEOF_UINT16_T], [The number of bytes in a uint16_t.])
AH_TEMPLATE([SIZEOF_UINT32_T], [The number of bytes in a uint32_t.])
AH_TEMPLATE([SIZEOF_UINT64_T], [The number of bytes in a uint64_t.])
AC_CHECK_SIZEOF_STDC_HEADERS(uint8_t, 0)
AC_CHECK_SIZEOF_STDC_HEADERS(uint16_t, 0)
AC_CHECK_SIZEOF_STDC_HEADERS(uint32_t, 0)
AC_CHECK_SIZEOF_STDC_HEADERS(uint64_t, 0)

dnl Checks for functions
AC_CHECK_FUNCS(mmap strerror strtoul mbrtowc mkstemp getopt_long utimes utime wcwidth)

# XXX Choose /usr/lib or /usr/lib64 for library installs.
MARK64=
case "${target_cpu}" in
x86_64*|powerpc64*|ppc64*|sparc64*|s390x*)        MARK64=64 ;;
esac
AC_SUBST(MARK64)

dnl Checks for libraries
AC_CHECK_LIB(z,gzopen)
dnl AC_CHECK_LIB(bz2, BZ2_bzReadOpen)

AC_CONFIG_FILES(Makefile src/Makefile magic/Makefile doc/Makefile)
AC_OUTPUT
