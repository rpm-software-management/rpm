# Process this file with autoconf to produce a configure script.

AC_INIT(beecrypt, 2.2.0, bob.deblier@pandora.be)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([beecrypt.h])
AM_CONFIG_HEADER([config.h])
AC_LIBTOOL_WIN32_DLL

# Checks for package options
AC_ARG_ENABLE(debug, [  --enable-debug          creates debugging code [default=no]],
  [ if test "$enableval" = no; then
      ac_enable_debug=no
    else
      ac_enable_debug=yes
    fi
  ], [ ac_enable_debug=no])

AC_ARG_ENABLE(optimized, [  --enable-optimized      enables the assembler optimizations [default depends on --enable-debug value]],
  [ if test "$enableval" = no; then
      ac_enable_optimized=no
    else
      ac_enable_optimized=yes
   fi
  ],
  [ if test "$ac_enable_debug" = no; then
      ac_enable_optimized=yes
    else
      ac_enable_optimized=no
    fi
  ])

AC_ARG_ENABLE(threads, [  --enable-threads        enables multithread support [de fault=yes]],
  [ if test "$enableval" = no; then
      ac_enable_threads=no
    else
      ac_enable_threads=yes
    fi
  ], [ ac_enable_threads=yes ])

AH_TEMPLATE([ENABLE_THREADS],[Define to 1 if you want to enable multithread support])

if test "$ac_enable_threads" = yes; then
  AC_DEFINE([ENABLE_THREADS],1)
fi

AC_ARG_ENABLE(aio, [  --enable-aio            enables asynchronous i/o for entropy gathering [default=yes]],
  [ if test "$enableval" = no; then
      ac_enable_aio=no
    else
      ac_enable_aio=yes
    fi
  ], [ ac_enable_aio=yes ])

AH_TEMPLATE([ENABLE_AIO],[Define to 1 if you want to enable asynchronous I/O support])

if test "$ac_enable_aio" = yes; then
  AC_DEFINE([ENABLE_AIO],1)
fi

AC_ARG_WITH(mtmalloc, [  --with-mtmalloc         links against the mtmalloc library [default=no]],

  [ if test "$withval" = no; then
      ac_with_mtmalloc=no
    else
      ac_with_mtmalloc=yes
    fi
  ], [ ac_with_mtmalloc=no ])

AC_ARG_WITH(javaglue, [  --with-javaglue         creates the java glue code [default=yes]],
  [ if test "$withval" = no; then
      ac_with_javaglue=no
    else
      ac_with_javaglue=yes
    fi
  ], [ ac_with_javaglue=yes ])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_SYS_LARGEFILE

# Initialize tool flags
CFLAGS=""
CPPFLAGS=""
LDFLAGS=""

# Checks for compiler characteristics and flags
if test "$ac_cv_prog_gcc" != yes; then
  AC_CACHE_CHECK([whether we are using Sun Workshop C], ac_cv_prog_SUNPRO_CC, [
    AC_EGREP_CPP(yes, [
      #ifdef __SUNPRO_C
        yes;
      #endif
      ], ac_cv_prog_SUNPRO_CC=yes, ac_cv_prog_SUNPRO_CC=no)
    ])
  if test "$ac_cv_prog_SUNPRO_CC" != yes; then
    AC_CACHE_CHECK([whether we are using Compaq's C compiler], ac_cv_prog_DECC, [
      AC_EGREP_CPP(yes, [
        #ifdef __DECC
          yes;
        #endif
        ], ac_cv_prog_DECC=yes, ac_cv_prog_DECC=no)
      ])
  fi
fi

# Predefines for autoheader
AH_TOP([
#ifndef _REENTRANT
#define _REENTRANT
#endif
])

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
#endif])

case $target_os in
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

AH_TEMPLATE([HAVE_LONG_LONG],[Define to 1 if your compiler knows the 'long long' type])
AH_TEMPLATE([HAVE_UNSIGNED_LONG_LONG],[Define to 1 if your compiler knows the 'unsigned long long' type])

dnl AH_TEMPLATE([INT8_TYPE],[Define to 8-bit signed integer type])
dnl AH_TEMPLATE([INT16_TYPE],[Define to 16-bit signed integer type])
dnl AH_TEMPLATE([INT32_TYPE],[Define to 32-bit signed integer type])
dnl AH_TEMPLATE([INT64_TYPE],[Define to 64-bit signed integer type])

dnl AH_TEMPLATE([UINT8_TYPE],[Define to 8-bit unsigned integer type])
dnl AH_TEMPLATE([UINT16_TYPE],[Define to 16-bit unsigned integer type])
dnl AH_TEMPLATE([UINT32_TYPE],[Define to 32-bit unsigned integer type])
dnl AH_TEMPLATE([UINT64_TYPE],[Define to 64-bit unsigned integer type])

dnl AH_TEMPLATE([FLOAT4_TYPE],[Define to 32-bit floating point type])
dnl AH_TEMPLATE([DOUBLE8_TYPE],[Define to 64-bit floating point type])

AH_BOTTOM([
#if LINUX
#define _LIBC_REENTRANT
#endif
])

# Checks for header files.
AC_CHECK_HEADERS([time.h sys/time.h])
AC_HEADER_TIME
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h malloc.h termio.h termios.h])
AC_CHECK_HEADERS([sys/ioctl.h sys/audioio.h sys/soundcard.h])

if test "$ac_enable_threads" = yes; then
  case $target_os in
    freebsd*)
      CFLAGS="$CFLAGS -pthread"
      CPPFLAGS="$CPPFLAGS -pthread"
      LDFLAGS="$LDFLAGS -pthread"
      ;;
    osf*)
      if test "$ac_cv_prog_DECC" = yes; then
        CFLAGS="$CFLAGS -pthread"
        CPPFLAGS="$CPPFLAGS -pthread"
      fi
      ;;
    solaris*)
      if test "$ac_cv_prog_SUNPRO_CC" = yes; then
        CFLAGS="$CFLAGS -mt"
      fi
      ;;
  esac
  AC_CHECK_HEADERS([thread.h pthread.h synch.h semaphore.h])
fi

if test "$ac_enable_aio" = yes; then
  AC_CHECK_HEADERS(aio.h)
fi

if test "$ac_with_mtmalloc" = yes; then
  AC_CHECK_HEADERS(mtmalloc.h)
fi

# Checks for libraries.
if test "$ac_enable_threads" = yes; then
  if test "$ac_cv_header_thread_h" = yes; then
    AC_HAVE_LIBRARY([thread])
  elif test "$ac_cv_header_pthread_h" = yes; then
    AC_HAVE_LIBRARY([pthread])
  fi
fi

if test "$ac_enable_aio" = yes; then
  if test "$ac_cv_header_aio_h" = yes; then
    AC_SEARCH_LIBS(aio_read, [c rt aio posix4], [ ], [
      AC_MSG_ERROR([no library containing aio routines found])
    ])
    AC_CACHE_CHECK([whether aio works], ac_cv_have_working_aio, [
      cat > conftest.aio << EOF
The quick brown fox jumps over the lazy dog.
EOF
      AC_TRY_RUN([
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
#include <aio.h>
#include <stdio.h>

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
    exit(1);

  if (aio_suspend(&a_list, 1, &a_timeout) < 0)
  {
    #if HAVE_ERRNO_H
    /* some linux systems don't await timeout and return instantly */
    if (errno == EAGAIN)
    {
      nanosleep(&a_timeout, (struct timespec*) 0);
      if (aio_suspend(&a_list, 1, &a_timeout) < 0)
        exit(1);
    }
    else
      exit(1);
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
        ], ac_cv_have_working_aio=yes, ac_cv_have_working_aio=no)
      ])
    if test "$ac_cv_have_working_aio" = yes; then
      AC_DEFINE([ENABLE_AIO], 1)
    fi
    rm -fr conftest*
  fi
fi

if test "$ac_with_mtmalloc" = yes; then
  if test "$ac_cv_have_mtmalloc_h" = yes; then
    AC_HAVE_LIBRARY([mtmalloc])
  fi
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(unsigned char)
if test "$ac_cv_sizeof_char" -eq 1; then
  AC_SUBST(int8_type, "char")
else
  AC_MSG_ERROR([compiler has no 1 byte char])
fi
if test "$ac_cv_sizeof_unsigned_char" -eq 1; then
  AC_SUBST(uint8_type, "unsigned char")
else
  AC_MSG_ERROR([compiler has no 1 byte unsigned char])
fi

AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(unsigned short)
if test "$ac_cv_sizeof_short" -eq 2; then
  AC_SUBST(int16_type, "short")
else
  AC_MSG_ERROR([compiler has no 2 byte short])
fi
if test "$ac_cv_sizeof_unsigned_short" -eq 2; then
  AC_SUBST(uint16_type, "unsigned short")
else
  AC_MSG_ERROR([compiler has no 2 byte unsigned short])
fi

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(unsigned long)
if test "$ac_cv_sizeof_int" -eq 4; then
  AC_SUBST(int32_type, "int")
elif test "$ac_cv_sizeof_long" -eq 4; then
  AC_SUBST(int32_type, "long")
else
  AC_MSG_ERROR([compiler has no 4 byte integer])
fi
if test "$ac_cv_sizeof_unsigned_int" -eq 4; then
  AC_SUBST(uint32_type, "unsigned int")
elif test "$ac_cv_sizeof_unsigned_long" -eq 4; then
  AC_SUBST(uint32_type, "unsigned long")
else
  AC_MSG_ERROR([compiler has no 4 byte unsigned integer])
fi

if test "$ac_cv_sizeof_long" -eq 8; then
  AC_SUBST(int64_type, "long")
else
  AC_CHECK_SIZEOF(long long)
  if test "$ac_cv_sizeof_long_long" -eq 8; then
    AC_DEFINE(HAVE_LONG_LONG)
    AC_SUBST(int64_type, "long long")
  else
    AC_MSG_ERROR([compiler has no 8 byte integer])
  fi
fi
if test "$ac_cv_sizeof_unsigned_long" -eq 8; then
  AC_SUBST(uint64_type, "unsigned long")
else
  AC_CHECK_SIZEOF(unsigned long long)
  if test "$ac_cv_sizeof_unsigned_long_long" -eq 8; then
    AC_DEFINE(HAVE_UNSIGNED_LONG_LONG)
    AC_SUBST(uint64_type, "unsigned long long")
  else
    AC_MSG_ERROR([compiler has no 8 byte unsigned integer])
  fi
fi

AC_CHECK_SIZEOF(float)
if test "$ac_cv_sizeof_float" -eq 4; then
  AC_SUBST(float4_type, "float")
else
  AC_MSG_ERROR([compiler has no 4 byte float])
fi

AC_CHECK_SIZEOF(double)
if test "$ac_cv_sizeof_double" -eq 8; then
  AC_SUBST(double8_type, "double")
else
  AC_MSG_ERROR([compiler has no 8 byte double])
fi

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_CHECK_FUNCS([memset memcmp memmove strcspn strerror strspn])

AH_TEMPLATE([HAVE_GETHRTIME],[.])
AH_TEMPLATE([HAVE_GETTIMEOFDAY],[.])
if test "$ac_cv_header_sys_time_h" = yes; then
  AC_CHECK_FUNCS([gethrtime gettimeofday])
fi

# Checks for symbol names (used in the assembler symbol names)
AH_TEMPLATE([LEADING_UNDERSCORE],[Define to 1 if your system compiles object names with leading underscores])

dnl XXX Choose /usr/lib or /usr/lib64 for library installs.
MARK64=
case "${target_cpu}" in
x86_64*|powerpc64*|ppc64*|sparc64*|s390x*)        MARK64=64 ;;
esac
AC_SUBST(MARK64)

case $target_os in
  cygwin* | darwin*)
    AC_DEFINE([LEADING_UNDERSCORE])
    ;;
esac

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
      if test -d "$ac_cv_java_include" ; then
        ac_cv_java_headers=yes
      else
	case $target_os in
	linux*)
	  gcjpath="/usr/lib/gcc-lib/`gcj -dumpmachine`/`gcj -dumpversion`"
	  if test -f $gcjpath/include/jni.h; then
	    ac_cv_java_include="$gcjpath/include"
	  fi
          ;;
        *)
          ac_cv_java_include="$java_home"/../include
          ;;
        esac
        if test "X$ac_cv_java_include" != X -a -d "$ac_cv_java_include" ; then
          ac_cv_java_headers=yes
        else
          AC_MSG_WARN([java headers not found, disabling javaglue])
          ac_cv_java_headers=no
          ac_with_javaglue=no
        fi
      fi
      rm -fr conftest*
    ])
  else
    AC_CHECK_PROG(ac_cv_have_java, java, yes, no)
    if test "$ac_cv_have_java" = yes; then
      AC_CHECK_PROG(ac_cv_have_javac, javac, yes, no)
      if test "$ac_cv_have_javac" = yes; then
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
          java_home=`javac conftest.java; java -classpath . conftest`
          case $target_os in
          cygwin*)
            java_home=`cygpath -u -p "$java_home"`
            ;;
          esac
          if test -d "$ac_cv_java_include" ; then
            ac_cv_java_headers=yes
          else
            case $target_os in
            darwin*)
              ac_cv_java_include="$java_home"/../../../Headers
              ;;
            *)
              ac_cv_java_include="$java_home"/../include
            esac
            if test -d "$ac_cv_java_include" ; then
              ac_cv_java_headers=yes
            else
              AC_MSG_WARN([java headers not found, disabling javaglue])
              ac_cv_java_headers=no
              ac_with_javaglue=no
            fi
          fi
          rm -fr conftest*
        ])
      else
        AC_MSG_WARN([javac not found, disabling javaglue])
        ac_cv_java_headers=no
        ac_with_javaglue=no
      fi
    fi
  fi
fi

if test "$ac_with_javaglue" = yes ; then
  AC_DEFINE([JAVAGLUE], 1)
fi

# Checks for entropy sources.
AC_CHECKING(for specific entropy devices)
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
    AC_CACHE_CHECK([for /dev/dsp], ac_cv_have_dev_dsp, [
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
    AC_CACHE_CHECK([for /dev/audio], ac_cv_have_dev_audio, [
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
    AC_CHECKING(for generic entropy devices)
    AC_CACHE_CHECK([for /dev/random], ac_cv_have_dev_random, [
      if test -r /dev/random; then
        ac_cv_have_dev_random=yes
      else
        ac_cv_have_dev_random=no
      fi
      ])
    if test "$ac_cv_have_dev_random" = yes; then
      AC_DEFINE([HAVE_DEV_RANDOM], 1)
    fi
    AC_CACHE_CHECK([for /dev/urandom], ac_cv_have_dev_urandom, [
      if test -r /dev/urandom; then
        ac_cv_have_dev_urandom=yes
      else
        ac_cv_have_dev_urandom=no
      fi
      ])
    if test "$ac_cv_have_dev_urandom" = yes; then
      AC_DEFINE([HAVE_DEV_URANDOM], 1)
    fi
    AC_CACHE_CHECK([for /dev/tty], ac_cv_have_dev_tty, [
      if test -r /dev/tty; then
        ac_cv_have_dev_tty=yes
      else
        ac_cv_have_dev_tty=no
      fi
      ])
    if test "$ac_cv_have_dev_tty" = yes; then
      AC_DEFINE([HAVE_DEV_TTY], 1)
    fi
    ;;
esac

if test "$ac_enable_optimized" = yes; then
  case $target_cpu in
  alpha*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_ALPHA"
    ;;
  arm*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_ARM"
    ;;
  ia64)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_IA64"
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
  powerpc)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_POWERPC"
    ;;
  sparcv8)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV8"
    ;;
  sparcv8plus*)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV8PLUS"
    CFLAGS="$CFLAGS -Wa,-xarch=v8plus"
    ;;
  sparcv9)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV9"
    CFLAGS="$CFLAGS -Wa,-xarch=v9"
    ;;
  sparcv9a)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV9"
    CFLAGS="$CFLAGS -Wa,-xarch=v9a"
    ;;
  sparcv9b)
    CPPFLAGS="$CPPFLAGS -DOPTIMIZE_SPARCV9"
    CFLAGS="$CFLAGS -Wa,-xarch=v9b"
    ;;
  x86)
  # QNX Neutrino doesn't list the exact cpu type
    ;;
  esac
fi

if test "$ac_enable_debug" = yes; then
  CFLAGS="$CFLAGS -g"
  if test "$ac_cv_prog_gcc" = yes; then
    CFLAGS="$CFLAGS -Wall"
  elif test "$ac_cv_prog_SUNPRO_CC" = yes; then
    if test "$ac_enable_threads" = yes; then
      CFLAGS="$CFLAGS -mt"
    fi
  fi
else
  if test "$ac_cv_prog_gcc" = yes; then
    CFLAGS="$CFLAGS -O3 -g"
    if test "$ac_enable_optimized" = yes; then
      case $target_cpu in
# This switch makes the mp32 routines slower by about 10%, so it's disabled
#     alphaev6)
#       CFLAGS="$CFLAGS -mcpu=ev6"
#       ;;
      arm*)
        CFLAGS="$CFLAGS -fomit-frame-pointer"
        ;;
      i386)
        CFLAGS="$CFLAGS -march=i386 -mcpu=i686 -fomit-frame-pointer"
        ;;
      i486)
        CFLAGS="$CFLAGS -march=i386 -mcpu=i686 -fomit-frame-pointer"
        ;;
      i586)
        CFLAGS="$CFLAGS -march=i386 -mcpu=i686 -fomit-frame-pointer"
        ;;
      i686)
	CFLAGS="$CFLAGS -march=i386  -mcpu=i686 -fomit-frame-pointer"
        ;;
      sparc)
        CFLAGS="$CFLAGS -m32 -mtune=ultrasparc"
        ;;
      sparcv8)
        CFLAGS="$CFLAGS -mv8"
        ;;
# This actually performs WORSE on gcc up to 2.95.3!
#     sparcv8plus*)
#       CFLAGS="$CFLAGS -mv8plus"
#       ;;
      sparcv9*)
        CFLAGS="$CFLAGS -m32 -mcpu=ultrasparc"
        ;;
      sparc64)
        CFLAGS="$CFLAGS -m64 -mcpu=ultrasparc"
        ;;
      esac
    fi
  elif test "$ac_cv_prog_SUNPRO_CC" = yes; then
    CFLAGS="$CFLAGS -fast"
    if test "$ac_enable_threads" = yes; then
      CFLAGS="$CFLAGS -mt"
    fi
    if test "$ac_enable_optimized" = yes; then
      case $target_cpu in
      sparcv8)
        CFLAGS="$CFLAGS -xtarget=generic -xarch=v8"
        ;;
      sparcv8plus)
        CFLAGS="$CFLAGS -xtarget=generic -xarch=v8plus"
        ;;
      sparcv8plusa)
        CFLAGS="$CFLAGS -xtarget=generic -xarch=v8plusa"
        ;;
      sparcv8plusb)
        CFLAGS="$CFLAGS -xtarget=generic -xarch=v8plusb"
        ;;
      sparcv9)
        CFLAGS="-xarch=v9"
        CFLAGS="$CFLAGS -xtarget=generic64 -xarch=v9"
        LDFLAGS="$LDFLAGS -64"
        ;;
      sparcv9a)
        CFLAGS="-xarch=v9"
        CFLAGS="$CFLAGS -xtarget=generic64 -xarch=v9a"
        LDFLAGS="$LDFLAGS -64"
        ;;
      sparcv9b)
        CFLAGS="-xarch=v9"
        CFLAGS="$CFLAGS -xtarget=generic64 -xarch=v9b"
        LDFLAGS="$LDFLAGS -64"
        ;;
      esac
    fi
  elif test "$ac_cv_prog_DECC" = yes; then
    CFLAGS="$CFLAGS -fast"
    if test "$ac_enable_threads" = yes; then
      CFLAGS="$CFLAGS -pthread"
    fi
  else
    CFLAGS="$CFLAGS -O2"
  fi
fi

if test "$ac_with_javaglue" = yes ; then
  if test "$ac_cv_have_gcj" = no; then
    CFLAGS="$CFLAGS -I$ac_cv_java_include"
  fi
  case $target_os in
  cygwin*)
    CFLAGS="$CFLAGS -I$ac_cv_java_include"/win32
    ;;
  darwin*)
    ;;
  linux*)
    if test "$ac_cv_have_gcj" = no; then
      CFLAGS="$CFLAGS -I$ac_cv_java_include"/linux
    fi
    ;;
  osf*)
    CFLAGS="$CFLAGS -I$ac_cv_java_include"/osf
    ;;
  solaris*)
    CFLAGS="$CFLAGS -I$ac_cv_java_include"/solaris
    ;;
  *)
    AC_MSG_WARN([please add appropriate -I$ac_cv_java_include/<operating system> flag])
    ;;
  esac
fi

# Generate the assembler optimizations, or empty stub files.
rm -f $srcdir/aesopt.c $srcdir/aesopt.S
rm -f $srcdir/blowfishopt.c $srcdir/blowfishopt.S
rm -f $srcdir/mp32opt.c $srcdir/mp32opt.S
rm -f $srcdir/sha1opt.c $srcdir/sha1opt.S
if test "$ac_enable_optimized" = yes; then
  case $target_cpu in
    arm*)
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.arm.S"
EOF
      ;;
    i[[34]]86)
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.i386.S"
EOF
      ;;
    i[[56]]86)
      cat > $srcdir/aesopt.S << EOF
#include "gas/aesopt.i586.S"
EOF
      cat > $srcdir/blowfishopt.S << EOF
#include "gas/blowfishopt.i586.S"
EOF
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.i386.S"
EOF
      cat > $srcdir/sha1opt.S << EOF
#include "gas/sha1opt.i586.S"
EOF
      ;;
    ia64)
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.ia64.S"
EOF
      ;;
    powerpc)
      cat > $srcdir/aesopt.S << EOF
#include "gas/aesopt.powerpc.S"
EOF
      cat > $srcdir/blowfishopt.S << EOF
#include "gas/blowfishopt.powerpc.S"
EOF
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.powerpc.S"
EOF
      ;;
    sparcv8)
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.sparcv8.S"
EOF
      ;;
    sparcv8plus* | sparcv9*)
      cat > $srcdir/mp32opt.S << EOF
#include "gas/mp32opt.sparcv9.S"
EOF
      ;;
  esac
fi
if test ! -r $srcdir/aesopt.S; then
  cat > $srcdir/aesopt.c << EOF
EOF
fi
if test ! -r $srcdir/blowfishopt.S; then
  cat > $srcdir/blowfishopt.c << EOF
EOF
fi
if test ! -r $srcdir/mp32opt.S; then
  cat > $srcdir/mp32opt.c << EOF
EOF
fi
if test ! -r $srcdir/sha1opt.S; then
  cat > $srcdir/sha1opt.c << EOF
EOF
fi

# Generate output files.
AC_CONFIG_FILES([Makefile
                 Doxyfile
                 beecrypt.spec
                 types.h
                 docs/Makefile
                 gas/Makefile
                 masm/Makefile
                 mwerks/Makefile
                 tests/Makefile])
AC_OUTPUT
