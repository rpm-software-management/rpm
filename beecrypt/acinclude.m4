dnl  BeeCrypt specific autoconf macros

dnl  Copyright 2003 Bob Deblier <bob.deblier@pandora.be>
dnl
dnl  This file is part of the BeeCrypt crypto library
dnl  
dnl
dnl  LGPL


dnl  BEECRYPT_INT_TYPES
AC_DEFUN(BEECRYPT_INT_TYPES,[
  AH_TEMPLATE([HAVE_INT64_T])
  AH_TEMPLATE([HAVE_UINT64_T])
  AC_CHECK_TYPE([int8_t],,[
    # Candidates are [char]
    AC_CHECK_SIZEOF([char])
    if test $ac_cv_sizeof_char -eq 1; then
      AC_DEFINE_UNQUOTED([int8_t],[char],[If not already defined, define as a signed integer of 8 bits])
    fi
    ])
  AC_CHECK_TYPE([int16_t],,[
    # Candidates are [short]
    AC_CHECK_SIZEOF([short])
    if test $ac_cv_sizeof_short -eq 2; then
      AC_DEFINE_UNQUOTED([int16_t],[short],[If not already defined, define as a signed integer of exactly 16 bits])
    fi
    ])
  AC_CHECK_TYPE([int32_t],,[
    # Candidates are [int]
    AC_CHECK_SIZEOF([int])
    if test $ac_cv_sizeof_int -eq 4; then
      AC_DEFINE_UNQUOTED([int32_t],[int],[If not already defined, define as a signed integer of exactly 32 bits])
    fi
    ])
  AC_CHECK_TYPE([int64_t],[
    AC_DEFINE([HAVE_INT64_T],1)
    ],[
    # Candidates are [long] and [long long]
    AC_CHECK_SIZEOF([long])
    if test $ac_cv_sizeof_long -eq 8; then
      AC_DEFINE_UNQUOTED([int64_t],[long],[If not already defined, define as a signed integer of exactly 64 bits])
    else
      AC_CHECK_SIZEOF([long long])
      if test $ac_cv_sizeof_long_long -eq 8; then
        AC_DEFINE_UNQUOTED([int64_t],[long long],[If not already defined, define as a signed integer of exactly 64 bits])
        AC_DEFINE([HAVE_INT64_T],1)
      fi
    fi
    ])
  AC_CHECK_TYPE([uint8_t],,[
    # Candidates are [unsigned char]
    AC_CHECK_SIZEOF([unsigned char])
    if test $ac_cv_sizeof_unsigned_char -eq 1; then
      AC_DEFINE_UNQUOTED([uint8_t],[unsigned char],[If not already defined, define as an unsigned integer of 8 bits])
    fi
    ])
  AC_CHECK_TYPE([uint16_t],,[
    # Candidates are [unsigned short]
    AC_CHECK_SIZEOF([unsigned short])
    if test $ac_cv_sizeof_unsigned_short -eq 2; then
      AC_DEFINE_UNQUOTED([uint16_t],[unsigned short],[If not already defined, define as an unsigned integer of exactly 16 bits])
    fi
    ])
  AC_CHECK_TYPE([uint32_t],,[
    # Candidates are [unsigned int]
    AC_CHECK_SIZEOF([unsigned int])
    if test $ac_cv_sizeof_unsigned_int -eq 4; then
      AC_DEFINE_UNQUOTED([uint32_t],[unsigned int],[If not already defined, define as an unsigned integer of exactly 32 bits])
    fi
    ])
  AC_CHECK_TYPE([uint64_t],[
    AC_DEFINE([HAVE_UINT64_T],1)
    ],[
    # Candidates are [unsigned long] and [unsigned long long]
    AC_CHECK_SIZEOF([unsigned long])
    if test $ac_cv_sizeof_unsigned_long -eq 8; then
      AC_DEFINE_UNQUOTED([uint64_t],[unsigned long],[If not already defined, define as an unsigned integer of exactly 64 bits])
    else
      AC_CHECK_SIZEOF([unsigned long long])
      if test $ac_cv_sizeof_unsigned_long_long -eq 8; then
        AC_DEFINE_UNQUOTED([uint64_t],[unsigned long long],[If not already defined, define as an unsigned integer of exactly 64 bits])
        AC_DEFINE([HAVE_UINT64_T],1)
      fi
    fi
    ])
  ])


dnl  BEECRYPT_CPU_BITS
AC_DEFUN(BEECRYPT_CPU_BITS,[
  AH_TEMPLATE([MP_WBITS],[Define to the word size of your CPU])
  AC_CHECK_SIZEOF([unsigned long])
  if test $ac_cv_sizeof_unsigned_long -eq 8; then
    AC_DEFINE([MP_WBITS],64)
  elif test $ac_cv_sizeof_unsigned_long -eq 4; then
    AC_DEFINE([MP_WBITS],32)
  else
    AC_MSG_ERROR([Illegal CPU word size])
  fi
  ])


dnl  BEECRYPT_WORKING_AIO
AC_DEFUN(BEECRYPT_WORKING_AIO,[
  AC_CHECK_HEADERS(aio.h)
  if test "$ac_cv_header_aio_h" = yes; then
    AC_SEARCH_LIBS([aio_read],[c rt aio posix4],,[
      AC_MSG_ERROR([no library containing aio routines found])
      ])
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
      ])
    rm -fr conftest.aio
  fi
  ])


dnl  BEECRYPT_CFLAGS_REM
AC_DEFUN(BEECRYPT_CFLAGS_REM,[
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


dnl  BEECRYPT_GNU_CC
AC_DEFUN(BEECRYPT_GNU_CC,[
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
    esac
  fi
  if test "$ac_enable_debug" = yes; then
    CFLAGS="$CFLAGS -Wall"
  else
    # Generic optimizations, including cpu tuning
    BEECRYPT_CFLAGS_REM([-g])
    BEECRYPT_CFLAGS_REM([-O2])
    CFLAGS="$CFLAGS -O3 -fomit-frame-pointer"
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
      # -mcpu=... doesn't work on ia64, and -O3 can lead to invalid code
      BEECRYPT_CFLAGS_REM([-O3])
      CFLAGS="$CFLAGS -O"
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
    powerpc)
      CFLAGS="$CFLAGS -mcpu=powerpc"
      ;;
    powerpc64)
      CFLAGS="$CFLAGS -mcpu=powerpc64"
      ;;
    sparcv8)
      CFLAGS="$CFLAGS -mv8"
      ;;
    sparcv8plus)
      CFLAGS="$CFLAGS -mv8plus"
      ;;
    esac
  fi
  ])


dnl  BEECRYPT_COMPAQ_CC
AC_DEFUN(BEECRYPT_COMPAQ_CC,[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_PROG_CPP])
  AC_CACHE_CHECK([whether we are using Compaq's C compiler],bc_cv_prog_COMPAQ_CC,[
    AC_EGREP_CPP(yes,[
      #ifdef __DECC
        yes;
      #endif
      ],bc_cv_prog_COMPAQ_CC=yes,bc_cv_prog_COMPAQ_CC=no)
    ])
  if test "$bc_cv_COMPAQ_CC" = yes; then
    if test "$ac_enable_threads" = yes; then
      CFLAGS="$CFLAGS -pthread"
      CPPFLAGS="$CPPFLAGS -pthread"
    fi
    if test "$ac_enable_debug" != yes; then
      CFLAGS="$CFLAGS -fast"
    fi
  fi
  ])


dnl  BEECRYPT_HPUX_CC
AC_DEFUN(BEECRYPT_HPUX_CC,[
  if test "$ac_enable_debug" != yes; then
    BEECRYPT_CFLAGS_REM([-g])
    CFLAGS="$CFLAGS -fast"
  fi
  ])


dnl  BEECRYPT_IBM_CC
AC_DEFUN(BEECRYPT_IBM_CC,[
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
      if test "$ac_with_arch" = yes; then
        CFLAGS="$CFLAGS -O5"
      else
        CFLAGS="$CFLAGS -O3"
      fi
    fi
    # Version 5.0 doesn't have this, but 6.0 does
    AC_CHECK_FUNC([__rotatel4])
  fi
  ])


dnl  BEECRYPT_INTEL_CC
AC_DEFUN(BEECRYPT_INTEL_CC,[
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
      CFLAGS="$CFLAGS -O3"
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
      i686 | pentiumpro | pentium[[23]])
        CFLAGS="$CFLAGS -tpp6"
        ;;
      pentium4)
        CFLAGS="$CFLAGS -tpp7"
        ;;
      esac
    fi
    AC_CHECK_FUNC([_rotl])
    AC_CHECK_FUNC([_rotr])
  fi
  ])


dnl  BEECRYPT_SUN_FORTE_CC
AC_DEFUN(BEECRYPT_SUN_FORTE_CC,[
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
  ])


dnl  BEECRYPT_ASM_DEFS
AC_DEFUN(BEECRYPT_ASM_DEFS,[
  AC_SUBST(ASM_OS,$target_os)
  AC_SUBST(ASM_CPU,$bc_target_cpu)
  AC_SUBST(ASM_ARCH,$bc_target_arch)
  AC_SUBST(ASM_BIGENDIAN,$ac_cv_c_bigendian)
  ])


dnl  BEECRYPT_ASM_TEXTSEG
AC_DEFUN(BEECRYPT_ASM_TEXTSEG,[
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
AC_DEFUN(BEECRYPT_ASM_GLOBL,[
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
AC_DEFUN(BEECRYPT_ASM_GSYM_PREFIX,[
  AC_CACHE_CHECK([if global symbols need leading underscore],
    bc_cv_asm_gsym_prefix,[
      case $target_os in
      cygwin* | darwin*) bc_cv_asm_gsym_prefix="_" ;;
      *)                 bc_cv_asm_gsym_prefix="" ;;
      esac
    ])
  AC_SUBST(ASM_GSYM_PREFIX,$bc_cv_asm_sym_prefix)
  ])


dnl  BEECRYPT_ASM_LSYM_PREFIX
AC_DEFUN(BEECRYPT_ASM_LSYM_PREFIX,[
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


dnl  BEECRYPT_ASM_SOURCES
AC_DEFUN(BEECRYPT_ASM_SOURCES,[
  echo > mpopt.s
  echo > aesopt.s
  echo > blowfishopt.s
  echo > sha1opt.s
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
    AC_CONFIG_COMMANDS([mpopt.x86],[
      m4 $srcdir/gas/mpopt.x86.m4 > mpopt.s
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
  esac
  if test "$ac_with_arch" = yes; then
    # Code is i586-specific!
    case $bc_target_arch in
    athlon* | i[[56]]86 | pentium*)
      AC_CONFIG_COMMANDS([aesopt.i586],[
        m4 $srcdir/gas/aesopt.i586.m4 > aesopt.s
        ])
      AC_CONFIG_COMMANDS([blowfishopt.i586],[
        m4 $srcdir/gas/blowfishopt.i586.m4 > blowfishopt.s
        ])
      AC_CONFIG_COMMANDS([sha1opt.i586],[
        m4 $srcdir/gas/sha1opt.i586.m4 > sha1opt.s
        ])
      ;;
    esac
  fi
  ])
