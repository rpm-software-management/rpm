dnl  BeeCrypt specific autoconf macros

dnl  Copyright 2003 Bob Deblier <bob.deblier@pandora.be>
dnl
dnl  This file is part of the BeeCrypt crypto library
dnl  
dnl
dnl  LGPL


dnl  BEECRYPT_WITH_CPU
AC_DEFUN([BEECRYPT_WITH_CPU],[
  ac_with_cpu=yes
  bc_target_cpu=$withval
  case $target_cpu in
  i[[3456]]86)
    case $withval in
    i[[3456]]86 | \
    pentium | pentium-mmx | pentiumpro | pentium[[234]] | \
    athlon | athlon-tbird | athlon-4 | athlon-xp | athlon-mp)
      ;;
    *)
      AC_MSG_WARN([invalid cpu type])
      bc_target_cpu=$target_cpu
      ;;
    esac
    ;;
  powerpc | powerpc64)
    case $withval in
    403 | 505 | \
    60[[1234]] | 60[[34]]e | 6[[23]]0 | \
    7[[45]]0 | 74[[05]]0 | \
    801 | 82[[13]] | 860 | \
    power | power2 | powerpc | powerpc64)
      ;;
    *)
      AC_MSG_WARN([invalid cpu type])
      bc_target_cpu=$target_cpu
      ;;
    esac
    ;;
  sparc | sparc64)
    case $withval in
    sparcv8 | sparcv8plus | sparcv8plus[[ab]] | sparcv9 | sparcv9[[ab]])
      ;;
    *)
      AC_MSG_WARN([invalid cpu type])
      bc_target_cpu=$target_cpu
      ;;
    esac
    ;;
  x86) # QNX Neutrino doesn't list the exact cpu type
    case $withval in
    i[[3456]]86)
      ;;
    *)
      AC_MSG_WARN([unsupported or invalid cpu type])
      bc_target_cpu=$target_cpu
      ;;
    esac
    ;;
  *)
    AC_MSG_WARN([unsupported or invalid cpu type])
    bc_target_cpu=$target_cpu
    ;;
  esac
  ])

dnl  BEECRYPT_WITHOUT_CPU
AC_DEFUN([BEECRYPT_WITHOUT_CPU],[
  ac_with_cpu=no
  bc_target_cpu=$target_cpu
  ])


dnl  BEECRYPT_WITH_ARCH
AC_DEFUN([BEECRYPT_WITH_ARCH],[
  ac_with_arch=yes
  bc_target_arch=$withval
  case $target_cpu in
  i[[3456]]86)
    case $withval in
    i[[3456]]86 | \
    pentium | pentium-mmx | pentiumpro | pentium[[234]] | \
    athlon | athlon-tbird | athlon-4 | athlon-xp | athlon-mp)
      if test "$ac_with_cpu" != yes; then
        bc_target_cpu=$withval
      fi
      ;;
    esac
    ;;
  powerpc*)
    case $withval in
    powerpc)
      ;;
    powerpc64)
      bc_target_arch=powerpc64
      ;;
    *)
      AC_MSG_WARN([unsupported on invalid arch type])
      bc_target_arch=powerpc
      ;;
    esac
    ;;
  esac
  ])

dnl  BEECRYPT_WITHOUT_ARCH
AC_DEFUN([BEECRYPT_WITHOUT_ARCH],[
  ac_with_arch=no
  case $target_cpu in
  alpha*)
    bc_target_arch=alpha
    ;;
  arm*)
    bc_target_arch=arm
    ;;
  i[[3456]]86)
    bc_target_arch=i386
    ;;
  ia64)
    bc_target_arch=ia64
    ;;
  m68k)
    bc_target_arch=m68k
    ;;
  powerpc)
    bc_target_arch=powerpc
    ;;
  powerpc64)
    bc_target_arch=powerpc64
    ;;
  s390x)
    bc_target_arch=s390x
    ;;
  sparc*)
    bc_target_arch=sparc
    ;;
  x86_64)
    bc_target_arch=x86_64
    ;;
  esac
  ])


dnl  BEECRYPT_INT_TYPES
AC_DEFUN([BEECRYPT_INT_TYPES],[
  AC_TYPE_SIZE_T
  bc_typedef_size_t=
  if test $ac_cv_type_size_t != yes; then
    bc_typedef_size_t="typedef unsigned size_t;"
  fi
  AC_SUBST(TYPEDEF_SIZE_T,$bc_typedef_size_t)
  if test $ac_cv_header_inttypes_h = yes; then
    AC_SUBST(INCLUDE_INTTYPES_H,["#include <inttypes.h>"])
  else
    AC_SUBST(INCLUDE_INTTYPES_H,[ ])
  fi
  if test $ac_cv_header_stdint_h = yes; then
    AC_SUBST(INCLUDE_STDINT_H,["#include <stdint.h>"])
  else
    AC_SUBST(INCLUDE_STDINT_H,[ ])
  fi
  AH_TEMPLATE([HAVE_INT64_T])
  AH_TEMPLATE([HAVE_UINT64_T])
  bc_typedef_int8_t=
  AC_CHECK_TYPE([int8_t],,[
    AC_CHECK_SIZEOF([signed char])
    if test $ac_cv_sizeof_signed_char -eq 1; then
      bc_typedef_int8_t="typedef signed char int8_t;"
    fi
    ])
  AC_SUBST(TYPEDEF_INT8_T,$bc_typedef_int8_t)
  bc_typedef_int16_t=
  AC_CHECK_TYPE([int16_t],,[
    AC_CHECK_SIZEOF([short])
    if test $ac_cv_sizeof_short -eq 2; then
      bc_typedef_int16_t="typedef short int16_t;"
    fi
    ])
  AC_SUBST(TYPEDEF_INT16_T,$bc_typedef_int16_t)
  bc_typedef_int32_t=
  AC_CHECK_TYPE([int32_t],,[
    AC_CHECK_SIZEOF([int])
    if test $ac_cv_sizeof_int -eq 4; then
      bc_typedef_int32_t="typedef int int32_t;"
    fi
    ])
  AC_SUBST(TYPEDEF_INT32_T,$bc_typedef_int32_t)
  bc_typedef_int64_t=
  AC_CHECK_TYPE([int64_t],[
    AC_DEFINE([HAVE_INT64_T],1)
    ],[
    AC_CHECK_SIZEOF([long])
    if test $ac_cv_sizeof_long -eq 8; then
      bc_typedef_int64_t="typedef long int64_t;"
    else
      AC_CHECK_SIZEOF([long long])
      if test $ac_cv_sizeof_long_long -eq 8; then
        AC_DEFINE([HAVE_INT64_T],1)
        bc_typedef_int64_t="typedef long long int64_t;"
      fi
    fi
    ])
  AC_SUBST(TYPEDEF_INT64_T,$bc_typedef_int64_t)
  bc_typedef_uint8_t=
  AC_CHECK_TYPE([uint8_t],,[
    AC_CHECK_SIZEOF([unsigned char])
    if test $ac_cv_sizeof_unsigned_char -eq 1; then
      bc_typedef_uint8_t="typedef unsigned char uint8_t;"
    fi
    ])
  AC_SUBST(TYPEDEF_UINT8_T,$bc_typedef_uint8_t)
  bc_typedef_uint16_t=
  AC_CHECK_TYPE([uint16_t],,[
    AC_CHECK_SIZEOF([unsigned short])
    if test $ac_cv_sizeof_unsigned_short -eq 2; then
      bc_typedef_uint16_t="typedef unsigned short uint16_t;"
    fi
    ])
  AC_SUBST(TYPEDEF_UINT16_T,$bc_typedef_uint16_t)
  bc_typedef_uint32_t=
  AC_CHECK_TYPE([uint32_t],,[
    AC_CHECK_SIZEOF([unsigned int])
    if test $ac_cv_sizeof_unsigned_int -eq 4; then
      bc_typedef_uint32_t="typedef unsigned int uint32_t;"
    fi
    ])
  AC_SUBST(TYPEDEF_UINT32_T,$bc_typedef_uint32_t)
  bc_typedef_uint64_t=
  AC_CHECK_TYPE([uint64_t],[
    AC_DEFINE([HAVE_UINT64_T],1)
    ],[
    AC_CHECK_SIZEOF([unsigned long])
    if test $ac_cv_sizeof_unsigned_long -eq 8; then
      bc_typedef_uint64_t="typedef unsigned long uint64_t;"
    else
      AC_CHECK_SIZEOF([unsigned long long])
      if test $ac_cv_sizeof_unsigned_long_long -eq 8; then
        AC_DEFINE([HAVE_UINT64_T],1)
        bc_typedef_uint64_t="typedef unsigned long long uint64_t;"
      fi
    fi
    ])
  AC_SUBST(TYPEDEF_UINT64_T,$bc_typedef_uint64_t)
  ])


dnl  BEECRYPT_CPU_BITS
AC_DEFUN([BEECRYPT_CPU_BITS],[
  AC_CHECK_SIZEOF([unsigned long])
  if test $ac_cv_sizeof_unsigned_long -eq 8; then
    AC_SUBST(MP_WBITS,64U)
  elif test $ac_cv_sizeof_unsigned_long -eq 4; then
    AC_SUBST(MP_WBITS,32U)
  else
    AC_MSG_ERROR([Illegal CPU word size])
  fi
  ])


dnl  BEECRYPT_WORKING_AIO
AC_DEFUN([BEECRYPT_WORKING_AIO],[
  AC_CHECK_HEADERS(aio.h)
  if test "$ac_cv_header_aio_h" = yes; then
    AC_SEARCH_LIBS([aio_read],[c rt aio posix4],[
      AC_CACHE_CHECK([whether aio works],bc_cv_working_aio,[
        cat > conftest.aio << EOF
The quick brown fox jumps over the lazy dog.
EOF
        AC_RUN_IFELSE([AC_LANG_SOURCE([[
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>
#include <aio.h>

main()
{
        struct aiocb    a;
  const struct aiocb*   a_list = &a;
        struct timespec a_timeout;

  char buffer[32];

  int i, rc, fd = open("conftest.aio", O_RDONLY);

  if (fd < 0)
    exit(1);

  memset(&a, 0, sizeof(struct aiocb));

  a.aio_fildes = fd;
  a.aio_offset = 0;
  a.aio_reqprio = 0;
  a.aio_buf = buffer;
  a.aio_nbytes = sizeof(buffer);
  a.aio_sigevent.sigev_notify = SIGEV_NONE;

  a_timeout.tv_sec = 1;
  a_timeout.tv_nsec = 0;

  if (aio_read(&a) < 0)
  {
    perror("aio_read");
    exit(1);
  }
  if (aio_suspend(&a_list, 1, &a_timeout) < 0)
  {
    #if HAVE_ERRNO_H
    /* some linux systems don't await timeout and return instantly */
    if (errno == EAGAIN)
    {
      nanosleep(&a_timeout, (struct timespec*) 0);

      if (aio_suspend(&a_list, 1, &a_timeout) < 0)
      {
        perror("aio_suspend");
        exit(1);
      }
    }
    else
    {
      perror("aio_suspend");
      exit(1);
    }
    #else
    exit(1);
    #endif
  }
  if (aio_error(&a) < 0)
    exit(1);

  if (aio_return(&a) < 0)
    exit(1);

  exit(0);
}
          ]])],[bc_cv_working_aio=yes],[bc_cv_working_aio=no],[
            case $target_os in
              linux* | solaris*)
                bc_cv_working_aio=yes ;;
              *)
                bc_cv_working_aio=no ;;
            esac
          ])
        ],[
        bc_cv_working_aio=no
        ])
      ])
    rm -fr conftest.aio
  fi
  ])


dnl  BEECRYPT_CFLAGS_REM
AC_DEFUN([BEECRYPT_CFLAGS_REM],[
  if test "$CFLAGS" != ""; then
    CFLAGS_save=""
    for flag in $CFLAGS
    do
      if test "$flag" != "$1"; then
        CFLAGS_save="$CFLAGS_save $flag"
      fi
    done
    CFLAGS="$CFLAGS_save"
  fi
  ])


dnl  BEECRYPT_CXXFLAGS_REM
AC_DEFUN([BEECRYPT_CXXFLAGS_REM],[
  if test "$CXXFLAGS" != ""; then
    CXXFLAGS_save=""
    for flag in $CXXFLAGS
    do
      if test "$flag" != "$1"; then
        CXXFLAGS_save="$CXXFLAGS_save $flag"
      fi
    done
    CXXFLAGS="$CXXFLAGS_save"
  fi
  ])


dnl  BEECRYPT_GNU_CC
AC_DEFUN([BEECRYPT_GNU_CC],[
  AC_REQUIRE([AC_PROG_CC])
  case $bc_target_arch in
  ia64)
    case $target_os in
    # HP/UX on Itanium needs to be told that a long is 64-bit!
    hpux*)
      CFLAGS="$CFLAGS -mlp64"
      ;;
    esac
    ;;
  # PowerPC needs a signed char
  powerpc)
    CFLAGS="$CFLAGS -fsigned-char"
    ;;
  powerpc64)
    CFLAGS="$CFLAGS -fsigned-char"
    case $target_os in
    aix*)
      CC="$CC -maix64"
      ;;
    esac
    ;;
  esac
  # Certain platforms needs special flags for multi-threaded code
  if test "$ac_enable_threads" = yes; then
    case $target_os in
    freebsd*)
      CFLAGS="$CFLAGS -pthread"
      CPPFLAGS="$CPPFLAGS -pthread"
      LDFLAGS="$LDFLAGS -pthread"
      ;;
    osf*)
      CFLAGS="$CFLAGS -pthread"
      CPPFLAGS="$CPPFLAGS -pthread"
      ;;
    esac
  fi
  if test "$ac_enable_debug" = yes; then
    BEECRYPT_CFLAGS_REM([-O2])
    CFLAGS="$CFLAGS -Wall -pedantic"
  else
    # Generic optimizations, including cpu tuning
    BEECRYPT_CFLAGS_REM([-g])
    BEECRYPT_CFLAGS_REM([-O2])
    CFLAGS="$CFLAGS -O3 -fomit-frame-pointer"
    if test "$bc_cv_c_aggressive_opt" = yes; then
      case $bc_target_cpu in
      athlon*)
        CFLAGS="$CFLAGS -mcpu=pentiumpro";
        ;;
      i586)
        CFLAGS="$CFLAGS -mcpu=pentium"
        ;;
      i686)
        CFLAGS="$CFLAGS -mcpu=pentiumpro"
        ;;
      ia64)
        # no -mcpu=... option on ia64
        ;;
      pentium*)
        CFLAGS="$CFLAGS -mcpu=$bc_target_arch"
        ;;
      esac
      # Architecture-specific optimizations
      case $bc_target_arch in
      athlon*)
        CFLAGS="$CFLAGS -march=$bc_target_arch"
        ;;
      i586)
        CFLAGS="$CFLAGS -march=pentium"
        ;;
      i686)
        CFLAGS="$CFLAGS -march=pentiumpro"
        ;;
      pentium*)
        CFLAGS="$CFLAGS -march=$bc_target_arch"
        ;;
      powerpc | powerpc64)
        CFLAGS="$CFLAGS -mcpu=$bc_target_arch"
        ;;
      sparcv8)
        CFLAGS="$CFLAGS -mv8"
        ;;
      sparcv8plus)
        CFLAGS="$CFLAGS -mv8plus"
        ;;
      esac
    fi
  fi
  ])


dnl  BEECRYPT_GNU_CXX
AC_DEFUN([BEECRYPT_GNU_CXX],[
  AC_REQUIRE([AC_PROG_CXX])
  case $bc_target_arch in
  ia64)
    case $target_os in
    # HP/UX on Itanium needs to be told that a long is 64-bit!
    hpux*)
      CXXFLAGS="$CXXFLAGS -mlp64"
      ;;
    esac
    ;;
  # PowerPC needs a signed char
  powerpc)
    CXXFLAGS="$CXXFLAGS -fsigned-char"
    ;;
  powerpc64)
    CXXFLAGS="$CXXFLAGS -fsigned-char"
    case $target_os in
    aix*)
      CXX="$CXX -maix64"
      ;;
    esac
    ;;
  esac
  # Certain platforms needs special flags for multi-threaded code
  if test "$ac_enable_threads" = yes; then
    case $target_os in
    freebsd*)
      CXXFLAGS="$CXXFLAGS -pthread"
      CXXCPPFLAGS="$CXXCPPFLAGS -pthread"
      LDFLAGS="$LDFLAGS -pthread"
      ;;
    osf*)
      CXXFLAGS="$CXXFLAGS -pthread"
      CXXCPPFLAGS="$CXXCPPFLAGS -pthread"
      ;;
    esac
  fi
  if test "$ac_enable_debug" = yes; then
    BEECRYPT_CXXFLAGS_REM([-O2])
    CXXFLAGS="$CXXFLAGS -Wall -pedantic"
  else
    # Generic optimizations, including cpu tuning
    BEECRYPT_CXXFLAGS_REM([-g])
    if test "$bc_cv_c_aggressive_opt" = yes; then
      case $bc_target_cpu in
      athlon*)
        CXXFLAGS="$CXXFLAGS -mcpu=pentiumpro";
        ;;
      i586)
        CXXFLAGS="$CXXFLAGS -mcpu=pentium"
        ;;
      i686)
        CXXFLAGS="$CXXFLAGS -mcpu=pentiumpro"
        ;;
      ia64)
        # no -mcpu=... option on ia64
        ;;
      pentium*)
        CXXFLAGS="$CXXFLAGS -mcpu=$bc_target_arch"
        ;;
      esac
      # Architecture-specific optimizations
      case $bc_target_arch in
      athlon*)
        CXXFLAGS="$CXXFLAGS -march=$bc_target_arch"
        ;;
      i586)
        CXXFLAGS="$CXXFLAGS -march=pentium"
        ;;
      i686)
        CXXFLAGS="$CXXFLAGS -march=pentiumpro"
        ;;
      pentium*)
        CXXFLAGS="$CXXFLAGS -march=$bc_target_arch"
        ;;
      powerpc | powerpc64)
        CXXFLAGS="$CXXFLAGS -mcpu=$bc_target_arch"
        ;;
      sparcv8)
        CXXFLAGS="$CXXFLAGS -mv8"
        ;;
      sparcv8plus)
        CXXFLAGS="$CXXFLAGS -mv8plus"
        ;;
      esac
    fi
  fi
  ])


dnl  BEECRYPT_COMPAQ_CC
AC_DEFUN([BEECRYPT_COMPAQ_CC],[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_PROG_CPP])
  AC_CACHE_CHECK([whether we are using Compaq's C compiler],bc_cv_prog_COMPAQ_CC,[
    AC_EGREP_CPP(yes,[
      #ifdef __DECC
        yes;
      #endif
      ],bc_cv_prog_COMPAQ_CC=yes,bc_cv_prog_COMPAQ_CC=no)
    ])
  if test "$bc_cv_prog_COMPAQ_CC" = yes; then
    if test "$ac_enable_threads" = yes; then
      CFLAGS="$CFLAGS -pthread"
      CPPFLAGS="$CPPFLAGS -pthread"
    fi
    if test "$ac_enable_debug" != yes; then
      BEECRYPT_CFLAGS_REM([-g])
      if test "$bc_cv_c_aggressive_opt" = yes; then
        CFLAGS="$CFLAGS -fast"
      fi
    fi
  fi
  ])


dnl  BEECRYPT_COMPAQ_CXX
AC_DEFUN([BEECRYPT_COMPAQ_CXX],[
  ])


dnl  BEECRYPT_HPUX_CC
AC_DEFUN([BEECRYPT_HPUX_CC],[
  if test "$ac_enable_debug" != yes; then
    BEECRYPT_CFLAGS_REM([-g])
    if test "$bc_cv_c_aggressive_opt" = yes; then
      CFLAGS="$CFLAGS -fast"
    fi
  fi
  ])


dnl  BEECRYPT_HP_CXX
AC_DEFUN([BEECRYPT_HP_CXX],[
  ])


dnl  BEECRYPT_IBM_CC
AC_DEFUN([BEECRYPT_IBM_CC],[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_PROG_CPP])
  AC_CACHE_CHECK([whether we are using IBM C],bc_cv_prog_IBM_CC,[
    AC_EGREP_CPP(yes,[
      #ifdef __IBMC__
        yes;
      #endif
      ],bc_cv_prog_IBM_CC=yes,bc_cv_prog_IBM_CC=no)
    ])
  if test "$bc_cv_prog_IBM_CC" = yes; then
    case $bc_target_arch in
    powerpc)
      CC="$CC -q32 -qarch=ppc"
      ;;
    powerpc64)
      CC="$CC -q64 -qarch=ppc64"
      ;;
    esac
    if test "$ac_enable_debug" != yes; then
      BEECRYPT_CFLAGS_REM([-g])
      if test "$bc_cv_c_aggressive_opt" = yes; then
        if test "$ac_with_arch" = yes; then
          CFLAGS="$CFLAGS -O5"
        else
          CFLAGS="$CFLAGS -O3"
        fi
      fi
    fi
    # Version 5.0 doesn't have this, but 6.0 does
    AC_CHECK_FUNC([__rotatel4])
  fi
  ])


dnl  BEECRYPT_IBM_CXX
AC_DEFUN([BEECRYPT_IBM_CXX],[
  ])


dnl  BEECRYPT_INTEL_CC
AC_DEFUN([BEECRYPT_INTEL_CC],[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_PROG_CPP])
  AC_CACHE_CHECK([whether we are using Intel C++],bc_cv_prog_INTEL_CC,[
    AC_EGREP_CPP(yes,[
      #ifdef __INTEL_COMPILER
        yes;
      #endif
      ],bc_cv_prog_INTEL_CC=yes,bc_cv_prog_INTEL_CC=no)
    ])
  if test "$bc_cv_prog_INTEL_CC" = yes; then
    if test "$ac_enable_debug" != yes; then
      BEECRYPT_CFLAGS_REM([-g])
      if test "$bc_cv_c_aggressive_opt" = yes; then
        case $bc_target_cpu in
        i586 | pentium | pentium-mmx)
          CFLAGS="$CFLAGS -mcpu=pentium"
          ;;
        i686 | pentiumpro | pentium[[23]])
          CFLAGS="$CFLAGS -mcpu=pentiumpro"
          ;;
        pentium4)
          CFLAGS="$CFLAGS -mcpu=pentium4"
          ;;
        esac
        case $bc_target_arch in
        i586 | pentium | pentium-mmx)
          CFLAGS="$CFLAGS -tpp5"
          ;;
        i686 | pentiumpro)
          CFLAGS="$CFLAGS -tpp6 -march=pentiumpro"
          ;;
        pentium2)
          CFLAGS="$CFLAGS -tpp6 -march=pentiumii"
          ;;
        pentium3)
          CFLAGS="$CFLAGS -tpp6 -march=pentiumiii"
          ;;
        pentium4)
          CFLAGS="$CFLAGS -tpp7 -march=pentium4"
          ;;
        esac
      fi
    fi
    AC_CHECK_FUNC([_rotl])
    AC_CHECK_FUNC([_rotr])
  fi
  ])


dnl  BEECRYPT_INTEL_CXX
AC_DEFUN([BEECRYPT_INTEL_CXX],[
  ])


dnl  BEECRYPT_SUN_FORTE_CC
AC_DEFUN([BEECRYPT_SUN_FORTE_CC],[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_PROG_CPP])
  AC_CACHE_CHECK([whether we are using Sun Forte C],bc_cv_prog_SUN_FORTE_CC,[
    AC_EGREP_CPP(yes,[
      #ifdef __SUNPRO_C
        yes;
      #endif
      ],bc_cv_prog_SUN_FORTE_CC=yes,bc_cv_prog_SUN_FORTE_CC=no)
    ])
  if test "$bc_cv_prog_SUN_FORTE_CC" = yes; then
    if test "$ac_enable_threads" = yes; then
      CFLAGS="$CFLAGS -mt"
    fi
    if test "$ac_enable_debug" != yes; then
      BEECRYPT_CFLAGS_REM([-g])
      if test "$bc_cv_c_aggressive_opt" = yes; then
        CFLAGS="$CFLAGS -fast"
        case $bc_target_arch in
        sparc)
          CFLAGS="$CFLAGS -xtarget=generic -xarch=generic"
          ;;
        sparcv8)
          CFLAGS="$CFLAGS -xtarget=generic -xarch=v8"
          ;;
        sparcv8plus*)
          CFLAGS="$CFLAGS -xtarget=generic -xarch=v8plus"
          ;;
        sparcv9*)
          CFLAGS="$CFLAGS -xtarget=generic64 -xarch=v9"
          ;;
        esac
      fi
    fi
  fi
  ])


dnl  BEECRYPT_SUN_FORTE_CXX
AC_DEFUN([BEECRYPT_SUN_FORTE_CXX],[
  ])


dnl  BEECRYPT_CC
AC_DEFUN([BEECRYPT_CC],[
  if test "$CFLAGS" = ""; then
    bc_cv_c_aggressive_opt=yes
  else
    bc_cv_c_aggressive_opt=no
  fi
  # set flags for large file support
  case $target_os in
  linux* | solaris*)
    CPPFLAGS="$CPPFLAGS `getconf LFS_CFLAGS`"
    LDFLAGS="$LDFLAGS `getconf LFS_LDFLAGS`"
    ;;
  esac
  if test "$ac_cv_c_compiler_gnu" = yes; then
    # Intel's icc can be mistakenly identified as gcc
    case $target_os in
    linux*)
      BEECRYPT_INTEL_CC
      ;;
    esac
    if test "$bc_cv_prog_INTEL_CC" != yes; then
      BEECRYPT_GNU_CC
    fi
  else
    case $target_os in
    aix*)
      BEECRYPT_IBM_CC
      ;;
    hpux*)
      BEECRYPT_HPUX_CC
      ;;
    linux*)
      BEECRYPT_INTEL_CC
      ;;
    solaris*)
      BEECRYPT_SUN_FORTE_CC
      ;;
    osf*)
      BEECRYPT_COMPAQ_CC
      ;;
    esac
  fi
  ])


dnl  BEECRYPT_CXX
AC_DEFUN([BEECRYPT_CXX],[
  if test "$CXXFLAGS" = ""; then
    bc_cv_cxx_aggressive_opt=yes
  else
    bc_cv_cxx_aggressive_opt=no
  fi
  if test "$ac_cv_cxx_compiler_gnu" = yes; then
    # Intel's icc can be mistakenly identified as gcc
    case $target_os in
    linux*)
      BEECRYPT_INTEL_CXX
      ;;
    esac
    if test "$bc_cv_prog_INTEL_CXX" != yes; then
      BEECRYPT_GNU_CXX
    fi
  else
    case $target_os in
    aix*)
      BEECRYPT_IBM_CXX
      ;;
    hpux*)
      BEECRYPT_HPUX_CXX
      ;;
    linux*)
      BEECRYPT_INTEL_CXX
      ;;
    solaris*)
      BEECRYPT_SUN_FORTE_CXX
      ;;
    osf*)
      BEECRYPT_COMPAQ_CXX
      ;;
    esac
  fi
  ])


dnl BEECRYPT_NOEXECSTACK
AC_DEFUN([BEECRYPT_NOEXECSTACK],[
  AC_CACHE_CHECK([whether the assembler can use noexecstack],bc_cv_as_noexecstack,[
    cat > conftest.c << EOF
void foo(void) { }
EOF
    if AC_TRY_COMMAND([$CC -c -o conftest.o conftest.c]) then
      bc_cv_as_noexecstack=yes
      if test "$ac_cv_c_compiler_gnu" = yes; then
         CFLAGS="$CFLAGS -Wa,--noexecstack"
      fi
      if test "$ac_cv_cxx_compiler_gnu" = yes; then
         CXXFLAGS="$CXXFLAGS -Wa,--noexecstack"
      fi
    else
      bc_cv_as_noexecstack=no
    fi
    ])
  AC_CACHE_CHECK([whether the linker can use noexecstack],bc_cv_ld_noexecstack,[
    if AC_TRY_COMMAND([$LD -z noexecstack -o conftest conftest.o]) then
      bc_cv_ld_noexecstack=yes
      LDFLAGS="$LDFLAGS -z noexecstack"
    else
      bc_cv_ld_noexecstack=no
    fi
    ]) 
  ])


dnl BEECRYPT_LIBTOOL
AC_DEFUN([BEECRYPT_LIBTOOL],[
  case $target_os in
  aix*)
    case $bc_target_arch in
    powerpc64)
      AR="ar -X64"
      NM="/usr/bin/nm -B -X64"
      ;;
    esac
    ;;
  solaris*)
    case $bc_target_arch in
    sparcv9*)
      LD="/usr/ccs/bin/ld -64"
      ;;
    esac
    ;;
  esac
  ])


dnl  BEECRYPT_OS_DEFS
AC_DEFUN([BEECRYPT_OS_DEFS],[
  AH_TEMPLATE([AIX],[Define to 1 if you are using AIX])
  AH_TEMPLATE([CYGWIN],[Define to 1 if you are using Cygwin])
  AH_TEMPLATE([DARWIN],[Define to 1 if you are using Darwin/MacOS X])
  AH_TEMPLATE([FREEBSD],[Define to 1 if you are using FreeBSD])
  AH_TEMPLATE([HPUX],[Define to 1 if you are using HPUX])
  AH_TEMPLATE([LINUX],[Define to 1 if you are using GNU/Linux])
  AH_TEMPLATE([NETBSD],[Define to 1 if you are using NetBSD])
  AH_TEMPLATE([OPENBSD],[Define to 1 if you are using OpenBSD])
  AH_TEMPLATE([OSF],[Define to 1 if you are using OSF])
  AH_TEMPLATE([QNX],[Define to 1 if you are using QNX])
  AH_TEMPLATE([SCO_UNIX],[Define to 1 if you are using SCO Unix])
  AH_TEMPLATE([SOLARIS],[Define to 1 if you are using Solaris])
  AH_VERBATIM([WIN32],[
#ifndef WIN32
 #undef WIN32
#endif
    ])

  case $target_os in
    aix*)
      AC_DEFINE([AIX])
      ;;
    cygwin*)
      AC_DEFINE([CYGWIN])
      AC_DEFINE([WIN32])
      ;;
    darwin*)
      AC_DEFINE([DARWIN])
      ;;
    freebsd*)
      AC_DEFINE([FREEBSD])
      ;;
    hpux*)
      AC_DEFINE([HPUX])
      ;;
    linux*)
      AC_DEFINE([LINUX])
      ;;
    netbsd*)
      AC_DEFINE([NETBSD])
      ;;
    openbsd*)
      AC_DEFINE([OPENBSD])
      ;;
    osf*)
      AC_DEFINE([OSF])
      ;;
    *qnx)
      AC_DEFINE([QNX])
      ;;
    solaris*)
      AC_DEFINE([SOLARIS])
      ;;
    sysv*uv*)
      AC_DEFINE([SCO_UNIX])
      ;;
    *)
      AC_MSG_WARN([Operating system type $target_os currently not supported and/or tested])
      ;;
  esac
  ])


dnl  BEECRYPT_ASM_DEFS
AC_DEFUN([BEECRYPT_ASM_DEFS],[
  AC_SUBST(ASM_OS,$target_os)
  AC_SUBST(ASM_CPU,$bc_target_cpu)
  AC_SUBST(ASM_ARCH,$bc_target_arch)
  AC_SUBST(ASM_BIGENDIAN,$ac_cv_c_bigendian)
  ])


dnl  BEECRYPT_ASM_TEXTSEG
AC_DEFUN([BEECRYPT_ASM_TEXTSEG],[
  AC_CACHE_CHECK([how to switch to text segment],
    bc_cv_asm_textseg,[
      case $target_os in
      aix*)
        bc_cv_asm_textseg=[".csect .text[PR]"] ;;
      hpux*)
        if test "$bc_target_arch" = ia64; then
          bc_cv_asm_textseg=[".section .text"]
        else
          bc_cv_asm_textseg=".code"
        fi
        ;;
      *)
        bc_cv_asm_textseg=".text" ;;
      esac
    ])
  AC_SUBST(ASM_TEXTSEG,$bc_cv_asm_textseg)
  ])


dnl  BEECRYPT_ASM_GLOBL
AC_DEFUN([BEECRYPT_ASM_GLOBL],[
  AC_CACHE_CHECK([how to declare a global symbol],
    bc_cv_asm_globl,[
      case $target_os in
      hpux*) bc_cv_asm_globl=".export" ;;
      *)     bc_cv_asm_globl=".globl" ;;
      esac
    ])
  AC_SUBST(ASM_GLOBL,$bc_cv_asm_globl)
  ])


dnl  BEECRYPT_ASM_GSYM_PREFIX
AC_DEFUN([BEECRYPT_ASM_GSYM_PREFIX],[
  AC_CACHE_CHECK([if global symbols need leading underscore],
    bc_cv_asm_gsym_prefix,[
      case $target_os in
      cygwin* | darwin*) bc_cv_asm_gsym_prefix="_" ;;
      *)                 bc_cv_asm_gsym_prefix="" ;;
      esac
    ])
  AC_SUBST(ASM_GSYM_PREFIX,$bc_cv_asm_gsym_prefix)
  ])


dnl  BEECRYPT_ASM_LSYM_PREFIX
AC_DEFUN([BEECRYPT_ASM_LSYM_PREFIX],[
  AC_CACHE_CHECK([how to declare a local symbol],
    bc_cv_asm_lsym_prefix,[
      case $target_os in
      aix* | darwin*) bc_cv_asm_lsym_prefix="L" ;;
      hpux* | osf*)   bc_cv_asm_lsym_prefix="$" ;;
      linux*)
        case $target_cpu in
        alpha*)       bc_cv_asm_lsym_prefix="$" ;;
        *)            bc_cv_asm_lsym_prefix=".L" ;;
        esac
        ;;
      *)              bc_cv_asm_lsym_prefix=".L" ;;
      esac
    ])
  AC_SUBST(ASM_LSYM_PREFIX,$bc_cv_asm_lsym_prefix)
  ])


dnl  BEECRYPT_ASM_ALIGN
AC_DEFUN([BEECRYPT_ASM_ALIGN],[
  AC_CACHE_CHECK([how to align symbols],
    bc_cv_asm_align,[
      case $target_cpu in
      alpha*)
        bc_cv_asm_align=".align 5" ;;
      i[[3456]]86 | athlon*)
        bc_cv_asm_align=".align 4" ;;
      ia64)
        bc_cv_asm_align=".align 16" ;;
      powerpc*)
        bc_cv_asm_align=".align 2" ;;
      s390x)
        bc_cv_asm_align=".align 4" ;;
      sparc*)
        bc_cv_asm_align=".align 4" ;;
      x86_64)
        bc_cv_asm_align=".align 16" ;;
      esac
    ])
  AC_SUBST(ASM_ALIGN,$bc_cv_asm_align)
  ])


dnl  BEECRYPT_ASM_SOURCES
AC_DEFUN([BEECRYPT_ASM_SOURCES],[
  echo > mpopt.s
  echo > aesopt.s
  echo > blowfishopt.s
  echo > sha1opt.s
  if test "$ac_enable_debug" != yes; then
    case $bc_target_arch in
    arm)
      AC_CONFIG_COMMANDS([mpopt.arm],[
        m4 $srcdir/gas/mpopt.arm.m4 > mpopt.s
        ])
      ;;
    alpha*)
      AC_CONFIG_COMMANDS([mpopt.alpha],[
        m4 $srcdir/gas/mpopt.alpha.m4 > mpopt.s
        ])
      ;;
    athlon* | i[[3456]]86 | pentium*)
      AC_CONFIG_COMMANDS([aesopt.x86],[
        m4 $srcdir/gas/aesopt.x86.m4 > aesopt.s
        ])
      AC_CONFIG_COMMANDS([mpopt.x86],[
        m4 $srcdir/gas/mpopt.x86.m4 > mpopt.s
        ])
      AC_CONFIG_COMMANDS([sha1opt.x86],[
        m4 $srcdir/gas/sha1opt.x86.m4 > sha1opt.s
        ])
      ;;
    ia64)
      AC_CONFIG_COMMANDS([mpopt.ia64],[
        m4 $srcdir/gas/mpopt.ia64.m4 > mpopt.s
        ])
      ;;
    m68k)
      AC_CONFIG_COMMANDS([mpopt.m68k],[
        m4 $srcdir/gas/mpopt.m68k.m4 > mpopt.s
        ])
      ;;
    powerpc)
      AC_CONFIG_COMMANDS([mpopt.ppc],[
        m4 $srcdir/gas/mpopt.ppc.m4 > mpopt.s
        ])
      AC_CONFIG_COMMANDS([blowfishopt.ppc],[
        m4 $srcdir/gas/blowfishopt.ppc.m4 > blowfishopt.s
        ])
      ;;
    powerpc64)
      AC_CONFIG_COMMANDS([mpopt.ppc64],[
        m4 $srcdir/gas/mpopt.ppc64.m4 > mpopt.s
        ])
      ;;
    s390x)
      AC_CONFIG_COMMANDS([mpopt.s390x],[
        m4 $srcdir/gas/mpopt.s390x.m4 > mpopt.s
        ])
      ;;
    sparcv8)
      AC_CONFIG_COMMANDS([mpopt.sparcv8],[
        m4 $srcdir/gas/mpopt.sparcv8.m4 > mpopt.s
        ])
      ;;
    sparcv8plus)
      AC_CONFIG_COMMANDS([mpopt.sparcv8plus],[
        m4 $srcdir/gas/mpopt.sparcv8plus.m4 > mpopt.s
        ])
      ;;
    x86_64)
      AC_CONFIG_COMMANDS([mpopt.x86_64],[
        m4 $srcdir/gas/mpopt.x86_64.m4 > mpopt.s
        ])
      ;;
    esac
    if test "$ac_with_arch" = yes; then
      # Code is i586-specific!
      case $bc_target_arch in
      athlon* | i[[56]]86 | pentium*)
        AC_CONFIG_COMMANDS([blowfishopt.i586],[
          m4 $srcdir/gas/blowfishopt.i586.m4 > blowfishopt.s
          ])
        ;;
      esac
    fi
  fi
  ])


dnl  BEECRYPT_DLFCN

AC_DEFUN([BEECRYPT_DLFCN],[
  AH_TEMPLATE([HAVE_DLFCN_H],[.])
  AC_CHECK_HEADERS([dlfcn.h])
  if test "$ac_cv_header_dlfcn_h" = yes; then
    AC_SEARCH_LIBS([dlopen],[dl dld],[
      ])
  fi
  ])


dnl  BEECRYPT_MULTITHREAD
AC_DEFUN([BEECRYPT_MULTITHREAD],[
  AH_TEMPLATE([ENABLE_THREADS],[Define to 1 if you want to enable multithread support])
  AH_TEMPLATE([HAVE_THREAD_H],[.])
  AH_TEMPLATE([HAVE_PTHREAD_H],[.])
  AH_TEMPLATE([HAVE_SYNCH_H],[.])
  AH_TEMPLATE([HAVE_SEMAPHORE_H],[.])

  if test "$ac_enable_threads" = yes; then
    AC_CHECK_HEADERS([synch.h thread.h pthread.h semaphore.h])
  fi

  bc_include_synch_h=
  bc_include_thread_h=
  bc_include_pthread_h=
  bc_typedef_bc_cond_t=
  bc_typedef_bc_mutex_t=
  bc_typedef_bc_thread_t=
  if test "$ac_enable_threads" = yes; then
    if test "$ac_cv_header_thread_h" = yes -a "$ac_cv_header_synch_h" = yes; then
      bc_include_synch_h="#include <synch.h>"
      bc_include_thread_h="#include <thread.h>"
      bc_typedef_bc_cond_t="typedef cond_t bc_cond_t;"
      bc_typedef_bc_mutex_t="typedef mutex_t bc_mutex_t;"
      bc_typedef_bc_thread_t="typedef thread_t bc_thread_t;"
      AC_SEARCH_LIBS([mutex_lock],[thread],[
        AC_DEFINE([ENABLE_THREADS],1)
        ])
    elif test "$ac_cv_header_pthread_h" = yes; then
      bc_include_pthread_h="#include <pthread.h>"
      bc_typedef_bc_cond_t="typedef pthread_cond_t bc_cond_t;"
      bc_typedef_bc_mutex_t="typedef pthread_mutex_t bc_mutex_t;"
      bc_typedef_bc_thread_t="typedef pthread_t bc_thread_t;"
      # On most systems this tests will say 'none required', but that doesn't
      # mean that the linked code will work correctly!
      case $target_os in
      linux* | solaris* )
        AC_DEFINE([ENABLE_THREADS],1)
        LIBS="-lpthread $LIBS"
        ;;
      osf*)
        AC_DEFINE([ENABLE_THREADS],1)
        LIBS="-lpthread -lmach -lexc $LIBS"
        ;;
      *)
        AC_SEARCH_LIBS([pthread_mutex_lock],[pthread],[
          AC_DEFINE([ENABLE_THREADS],1)
          ])
        ;;
      esac
    else
      AC_MSG_WARN([Don't know which thread library to check for])
    fi
  fi
  AC_SUBST(INCLUDE_SYNCH_H,$bc_include_synch_h)
  AC_SUBST(INCLUDE_THREAD_H,$bc_include_thread_h)
  AC_SUBST(INCLUDE_PTHREAD_H,$bc_include_pthread_h)
  AC_SUBST(TYPEDEF_BC_COND_T,$bc_typedef_bc_cond_t)
  AC_SUBST(TYPEDEF_BC_MUTEX_T,$bc_typedef_bc_mutex_t)
  AC_SUBST(TYPEDEF_BC_THREAD_T,$bc_typedef_bc_thread_t)
  ])
