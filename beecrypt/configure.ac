# Process this file with autoconf to produce a configure script.

AC_INIT([beecrypt],[3.0.0pre],[bob.deblier@pandora.be])
AM_INIT_AUTOMAKE
AC_CANONICAL_TARGET
AC_CONFIG_SRCDIR(beecrypt.h)
AC_CONFIG_HEADERS([config.h])
AC_LIBTOOL_WIN32_DLL

# Checks for package options
AC_ARG_ENABLE(debug, [  --enable-debug          creates debugging code [default=no]],[
  if test "$enableval" = no; then
    ac_enable_debug=no
  else
    ac_enable_debug=yes
  fi
  ],[ ac_enable_debug=no])

AC_ARG_WITH(cpu,[  --with-cpu              optimize for specific cpu],[
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
  powerpc)
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
  sparc)
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
  ],[
  ac_with_cpu=no
  bc_target_cpu=$target_cpu
  ])

AC_ARG_WITH(arch,[  --with-arch             optimize for specific architecture (may not run on other cpus of same family)],[
  ac_with_arch=yes
  bc_target_arch=$withval
  case $target_cpu in
  i[[3456]]86)
    case $withval in
    i[[3456]]86 | \
    pentium | pentium-mmx | pentiumpro | pentium[[234]] | \
    athlon | athlon-tbird | athlon-4 | athlon-xp | athlon-mp)
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
  ],[
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
  powerpc*)
    bc_target_arch=powerpc
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

AC_ARG_ENABLE(threads,[  --enable-threads        enables multithread support [default=yes]],[
  if test "$enableval" = no; then
    ac_enable_threads=no
  else
    ac_enable_threads=yes
  fi
  ],[ ac_enable_threads=yes])

AH_TEMPLATE([ENABLE_THREADS],[Define to 1 if you want to enable multithread support])

AC_ARG_ENABLE(aio,[  --enable-aio            enables asynchronous i/o for entropy gathering [default=yes]],[
  if test "$enableval" = no; then
    ac_enable_aio=no
  else
    ac_enable_aio=yes
  fi
  ],[ac_enable_aio=yes])

AH_TEMPLATE([ENABLE_AIO],[Define to 1 if you want to enable asynchronous I/O support])

AC_ARG_WITH(mtmalloc,[  --with-mtmalloc         links against the mtmalloc library [default=no]],[
  if test "$withval" = no; then
    ac_with_mtmalloc=no
  else
    ac_with_mtmalloc=yes
  fi
  ],[ac_with_mtmalloc=no])

AC_ARG_WITH(javaglue,[  --with-javaglue         creates the java glue code [default=yes]],[
  if test "$withval" = no; then
    ac_with_javaglue=no
  else
    ac_with_javaglue=yes
  fi
  ],[ac_with_javaglue=yes])

# Check for Unix variants
AC_AIX

dnl XXX Choose /usr/lib or /usr/lib64 for library installs.
MARK64=
case "${target_cpu}" in
x86_64*|powerpc64*|ppc64*|sparc64*|s390x*)        MARK64=64 ;;
esac
AC_SUBST(MARK64)

# Checks for C compiler and preprocessor
AC_PROG_CC
AC_PROG_CPP
AM_PROG_AS

# Checks for compiler characteristics and flags
if test "$ac_cv_c_compiler_gnu" = yes; then
  BEECRYPT_GNU_CC
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

# Checks for program flags needed by libtool
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

# Checks for further programs
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# Predefines for autoheader
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

AH_TEMPLATE([HAVE_CTYPE_H],[.])
AH_TEMPLATE([HAVE_ERRNO_H],[.])
AH_TEMPLATE([HAVE_FCNTL_H],[.])
AH_TEMPLATE([HAVE_MALLOC_H],[.])
AH_TEMPLATE([HAVE_STDLIB_H],[.])
AH_TEMPLATE([HAVE_STRING_H],[.])
AH_TEMPLATE([HAVE_TERMIO_H],[.])
AH_TEMPLATE([HAVE_TERMIOS_H],[.])
AH_TEMPLATE([HAVE_TIME_H],[.])
AH_TEMPLATE([HAVE_UNISTD_H],[.])

AH_TEMPLATE([HAVE_SYS_AUDIOIO_H],[.])
AH_TEMPLATE([HAVE_SYS_IOCTL_H],[.])
AH_TEMPLATE([HAVE_SYS_SOUNDCARD_H],[.])
AH_TEMPLATE([HAVE_SYS_STAT_H],[.])
AH_TEMPLATE([HAVE_SYS_TIME_H],[.])
AH_TEMPLATE([HAVE_SYS_TYPES_H],[.])

AH_TEMPLATE([HAVE_THREAD_H],[.])
AH_TEMPLATE([HAVE_PTHREAD_H],[.])
AH_TEMPLATE([HAVE_SYNCH_H],[.])
AH_TEMPLATE([HAVE_SEMAPHORE_H],[.])

AH_TEMPLATE([HAVE_AIO_H],[.])
AH_TEMPLATE([HAVE_MTMALLOC_H],[.])

AH_TEMPLATE([HAVE_DEV_AUDIO],[Define to 1 if your system has device /dev/audio])
AH_TEMPLATE([HAVE_DEV_DSP],[Define to 1 if your system has device /dev/dsp])
AH_TEMPLATE([HAVE_DEV_RANDOM],[Define to 1 if your system has device /dev/random])
AH_TEMPLATE([HAVE_DEV_URANDOM],[Define to 1 if your system has device /dev/urandom])
AH_TEMPLATE([HAVE_DEV_TTY],[Define to 1 if your system has device /dev/tty])

AH_BOTTOM([
#if ENABLE_THREADS
# ifndef _REENTRANT
#  define _REENTRANT
# endif
# if LINUX
#  define _LIBC_REENTRANT
# endif
#else
# ifdef _REENTRANT
#  undef _REENTRANT
# endif
#endif
])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([time.h sys/time.h])
AC_HEADER_TIME
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h malloc.h termio.h termios.h])
AC_CHECK_HEADERS([sys/ioctl sys/audioio.h sys/soundcard.h])

if test "$ac_enable_threads" = yes; then
  AC_CHECK_HEADERS([thread.h pthread.h synch.h semaphore.h])
fi

if test "$ac_with_mtmalloc" = yes; then
  AC_CHECK_HEADERS(mtmalloc.h)
fi

# Checks for libraries.
bc_include_synch_h=
bc_include_pthread_h=
bc_typedef_bc_lock_t=
if test "$ac_enable_threads" = yes; then
  if test "$ac_cv_header_thread_h" = yes -a "$ac_cv_header_synch_h" = yes; then
    AC_SEARCH_LIBS([mutex_lock],[thread],[
      AC_DEFINE([ENABLE_THREADS],1)
      bc_include_synch_h="#include <sync.h>"
      bc_typedef_bc_lock_t="typedef pthread_mutex_t bc_lock_t;"
      ])
  elif test "$ac_cv_header_pthread_h" = yes; then
    AC_SEARCH_LIBS([pthread_mutex_lock],[pthread],[
      AC_DEFINE([ENABLE_THREADS],1)
      bc_include_pthread_h="#include <pthread.h>"
      bc_typedef_bc_lock_t="typedef pthread_mutex_t bc_lock_t;"
      ])
  else
    AC_MSG_WARN([Don't know which thread library to check for])
  fi
fi
AC_SUBST(INCLUDE_SYNCH_H, $bc_include_synch_h)
AC_SUBST(INCLUDE_PTHREAD_H, $bc_include_pthread_h)
AC_SUBST(TYPEDEF_BC_LOCK_T, $bc_typedef_bc_lock_t)

if test "$ac_enable_aio" = yes; then
  BEECRYPT_WORKING_AIO
  if test "$bc_cv_aio_works" = yes; then
    AC_DEFINE([ENABLE_AIO],1)
  fi
fi

if test "$ac_with_mtmalloc" = yes; then
  if test "$ac_cv_have_mtmalloc_h" = yes; then
    AC_CHECK_LIB([mtmalloc],[main]) ac_cv_lib_mtmalloc=ac_cv_lib_mtmalloc_main

  fi
fi

case $target_os in
  cygwin*)
    AC_CHECK_LIB([winmm],[main]) ac_cv_lib_winmm=ac_cv_lib_winmm_main
    ;;
esac

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_CHECK_FUNCS([memset memcmp memmove strcspn strerror strspn])

AH_TEMPLATE([HAVE_GETHRTIME],[.])
AH_TEMPLATE([HAVE_GETTIMEOFDAY],[.])
if test "$ac_cv_header_sys_time_h" = yes; then
  AC_CHECK_FUNCS([gethrtime gettimeofday])
fi

# Predefines and checks for Java API support
AH_TEMPLATE([JAVAGLUE],[Define to 1 if you want to include the Java code])

if test "$ac_with_javaglue" = yes ; then
  AC_CHECK_PROG(ac_cv_have_gcj, gcj, yes, no)
  if test "$ac_cv_have_gcj" = yes; then
    AC_CACHE_CHECK([for java native interface headers], ac_cv_java_include, [
      cat > conftest.java << EOF
public class conftest
{
  public static void main(String[[]] argv)
  {
     System.out.println(System.getProperty("java.home"));
  }
}
EOF
      java_home="`gcj --main=conftest -o conftest conftest.java; ./conftest`"
      if test X"$java_home" = X; then
        java_home=/usr
      fi
      if test -d "$java_home" -a -d "$java_home/include" -a -f "$java_home/include/jni.h"; then
        ac_cv_java_headers=yes
        ac_cv_java_include="-I$java_home/include"
        gcjpath="$java_home/lib/gcc-lib/`gcj -dumpmachine`/`gcj -dumpversion`"
        if test -d "$gcjpath" -a -d "$gcjpath/include"; then
          ac_cv_java_include="$ac_cv_java_include -I$gcjpath/include"
        fi
      else
        AC_MSG_WARN([java headers not found, disabling javaglue])
        ac_cv_java_headers=no
        ac_cv_java_include=
        ac_with_javaglue=no
      fi
      rm -fr conftest*
    ])
  else
  AC_CHECK_PROG(ac_cv_have_java, java, yes, no)
  if test "$ac_cv_have_java" = yes; then
    AC_CHECK_PROG(ac_cv_have_javac, javac, yes, no)
    if test "$ac_cv_have_javac" = yes; then
      AC_CACHE_CHECK([for java native interface headers],ac_cv_java_include,[
        cat > conftest.java << EOF
public class conftest
{
  public static void main(String[[]] argv)
  {
     System.out.println(System.getProperty("java.home"));
  }
}
EOF
        java_home=`javac conftest.java; java -classpath . conftest`
        case $target_os in
        cygwin*)
          java_home=`cygpath -u -p "$java_home"` ;;
        esac
	if test -d "$java_home"; then
          case $target_os in
          darwin*)
	    java_include="$java_home/../../../Headers" ;;
          *)
	    java_include="$java_home"/../include ;;
          esac
	  if test -d "$java_include" -a -f "$java_include/jni.h"; then
            ac_cv_java_headers=yes
              ac_cv_java_include="-I$java_include"
              case $target_os in
              aix*)
                ac_cv_java_include="-I$java_include -I$java_include/aix" ;;
              cygwin*)
                ac_cv_java_include="-I$java_include -I$java_include/win32" ;;
              darwin*) ;;
              linux*)
                ac_cv_java_include="-I$java_include -I$java_include/linux" ;;
              osf*)
                ac_cv_java_include="-I$java_include -I$java_include/osf" ;;
              solaris*)
                ac_cv_java_include="-I$java_include -I$java_include/solaris" ;;
              *)
                AC_MSG_WARN([please add appropriate -I$java_include/<operating system> flag])
                ac_cv_java_include="-I$java_include" ;;
              esac
          else
            AC_MSG_WARN([java headers not found, disabling javaglue])
            ac_cv_java_headers=no
              ac_cv_java_include=
            ac_with_javaglue=no
          fi
        fi
        rm -fr conftest*
      ])
    else
      AC_MSG_WARN([javac not found, disabling javaglue])
      ac_cv_java_headers=no
        ac_cv_java_include=
        ac_with_javaglue=no
    fi
  else
    AC_MSG_WARN([java not found, disabling javaglue])
    ac_cv_java_headers=no
    ac_with_javaglue=no
  fi
fi
fi

if test "$ac_with_javaglue" = yes ; then
  AC_DEFINE([JAVAGLUE],1)
fi

# Checks for entropy sources.
AS_MESSAGE(checking for specific entropy devices...)
case $target_os in
cygwin*)
  AC_MSG_CHECKING([for wavein])
  AC_MSG_RESULT(yes)
  AC_MSG_CHECKING([for wincrypt])
  AC_MSG_RESULT(yes)
  AC_MSG_CHECKING([for console])
  AC_MSG_RESULT(yes)
  ;;
linux*)
  AC_CACHE_CHECK([for /dev/dsp],ac_cv_have_dev_dsp,[
    if test -r /dev/dsp; then
      ac_cv_have_dev_dsp=yes
    else
      ac_cv_have_dev_dsp=no
    fi
    ])
  if test "$ac_cv_have_dev_dsp" = yes; then
    AC_DEFINE([HAVE_DEV_DSP], 1)
  fi
  ;;
solaris*)
  AC_CACHE_CHECK([for /dev/audio],ac_cv_have_dev_audio,[
    if test -r /dev/audio; then
      ac_cv_have_dev_audio=yes
    else
      ac_cv_have_dev_audio=no
    fi
    ])
  if test "$ac_cv_have_dev_audio" = yes; then
    AC_DEFINE([HAVE_DEV_AUDIO], 1)
  fi
  ;;
*)
  AC_MSG_WARN(no specific entropy devices present)
  ;;
esac

case $target_os in
cygwin*)
  ;;
*)
  AS_MESSAGE(checking for generic entropy devices...)
  AC_CACHE_CHECK([for /dev/random],ac_cv_have_dev_random,[
    if test -r /dev/random; then
      ac_cv_have_dev_random=yes
    else
      ac_cv_have_dev_random=no
    fi
    ])
  AC_CACHE_CHECK([for /dev/urandom],ac_cv_have_dev_urandom,[
    if test -r /dev/urandom; then
      ac_cv_have_dev_urandom=yes
    else
      ac_cv_have_dev_urandom=no
    fi
    ])
  AC_CACHE_CHECK([for /dev/tty],ac_cv_have_dev_tty,[
    if test -r /dev/tty; then
      ac_cv_have_dev_tty=yes
    else
      ac_cv_have_dev_tty=no
    fi
    ])
  ;;
esac

if test "$ac_cv_have_dev_random" = yes; then
  AC_DEFINE([HAVE_DEV_RANDOM],1)
fi
if test "$ac_cv_have_dev_urandom" = yes; then
  AC_DEFINE([HAVE_DEV_URANDOM],1)
fi
if test "$ac_cv_have_dev_tty" = yes; then
  AC_DEFINE([HAVE_DEV_TTY],1)
fi

if test "$ac_enable_debug" != yes; then
  case $bc_target_arch in
  alpha*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_ALPHA"
    ;;
  arm*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_ARM"
    ;;
  athlon*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I386"
    ;;
  i386)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I386"
    ;;
  i486)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I486"
    ;;
  i586)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I586"
    ;;
  i686)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I686"
    ;;
  ia64)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_IA64"
    ;;
  m68k)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_M68K"
    ;;
  pentium | pentium-mmx)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I586"
    ;;
  pentiumpro | pentium[[234]])
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_I686"
    ;;
  powerpc)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_POWERPC"
    ;;
  powerpc64)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_POWERPC64"
    ;;
  s390x)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_S390X"
    ;;
  sparcv8)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV8"
    CFLAGS="$CFLAGS -Wa,-xarch=v8"
    ;;
  sparcv8plus*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV8PLUS"
    CFLAGS="$CFLAGS -Wa,-xarch=v8plus"
    ;;
  sparcv9*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV9"
    CFLAGS="$CFLAGS -Wa,-xarch=v9"
    ;;
  x86_64)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_X86_64"
    ;;
  esac
fi

if test "$ac_enable_debug" != yes; then
  # find out how to use assembler
  BEECRYPT_ASM_DEFS
  BEECRYPT_ASM_TEXTSEG
  BEECRYPT_ASM_GLOBL
  BEECRYPT_ASM_GSYM_PREFIX
  BEECRYPT_ASM_LSYM_PREFIX
  BEECRYPT_ASM_ALIGN
fi

  # generate assembler source files from m4 files
  BEECRYPT_ASM_SOURCES

# Check for standard types and integers of specific sizes
BEECRYPT_INT_TYPES
BEECRYPT_CPU_BITS

if test "$ac_with_javaglue" = yes ; then
  CFLAGS="$CFLAGS $ac_cv_java_include"
fi

# Generate output files.
AC_CONFIG_FILES([Makefile
                 Doxyfile
                 beecrypt.gnu.h
                 config.m4
                 docs/Makefile
                 gas/Makefile
                 masm/Makefile
                 mwerks/Makefile
                 python/Makefile
                 python/test/Makefile
                 tests/Makefile])
AC_OUTPUT
