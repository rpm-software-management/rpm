AC_PREREQ(2.59)
AC_INIT(zlib, 1.2.1.1, rpm-list@redhat.com)
AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([gzio.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([foreign])

dnl Check for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_SYS_LARGEFILE

CFLAGS="-DHAS_snprintf -DHAS_snprintf"
if test "$ac_cv_prog_gcc" = yes; then
    CFLAGS="-DWITH_RSYNC_PAD -O3 -fomit-frame-pointer -Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts"
fi
export CFLAGS

AC_CHECK_HEADERS(unistd.h errno.h)

AC_MSG_CHECKING(if mmap is available)
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h> ], [
caddr_t hello() {
  return mmap((caddr_t)0, (off_t)0, PROT_READ, MAP_SHARED, 0, (off_t)0); 
}
],
[AC_MSG_RESULT(yes)
   CFLAGS="-DUSE_MMAP $CFLAGS"],
[AC_MSG_RESULT(no)]
)

AC_OUTPUT([ Doxyfile Makefile ],
  [	echo timestamp > stamp-h.in ]
)
