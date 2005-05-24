AC_INIT(popt.h)
AC_CANONICAL_SYSTEM
AC_PREREQ(2.12)
AC_CONFIG_HEADERS
AM_INIT_AUTOMAKE(popt, 1.10.2)
AM_CONFIG_HEADER(config.h)

ALL_LINGUAS="cs da de es eu_ES fi fr gl hu id is it ja ko no pl pt pt_BR ro ru sk sl sr sv tr uk wa zh zh_CN zh_TW"

AC_ISC_POSIX

AC_PROG_CC
AC_GCC_TRADITIONAL
AM_C_PROTOTYPES
AC_SYS_LARGEFILE

dnl AM_DISABLE_SHARED
AM_PROG_LIBTOOL

AC_PROG_INSTALL

if test "X$CC" = Xgcc; then
    CFLAGS="-Wall $CFLAGS"
fi

dnl XXX lose rpm libs
LIBS=
addlib() {
  l=$1
  shift
  case "$target" in 
    *-*-solaris*)	LIBS="$LIBS -L$l -R $l $*";;
    *)		LIBS="$LIBS -L$l $*";;
  esac
}

dnl
dnl if CC is gcc, we can rebuild the dependencies (since the depend rule
dnl requires gcc).  If it's not, don't rebuild dependencies -- use what was
dnl shipped with RPM.
dnl
if test X"$GCC" = "Xyes" ; then
    TARGET="depend allprogs"
else
    TARGET="everything"
    #
    # let the Makefile know that we're done with `depend', since we don't
    # have gcc we're not going to rebuild our dependencies at all.
    #
    echo > .depend-done
fi
AC_SUBST(TARGET)

dnl XXX Choose /usr/lib or /usr/lib64 for library installs.
MARK64=
case "${target_cpu}" in
x86_64*|powerpc64*|ppc64*|sparc64*|s390x*)        MARK64=64 ;;
esac
AC_SUBST(MARK64)

AC_CHECK_HEADERS(alloca.h float.h libintl.h mcheck.h unistd.h)
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

if test ! -f ../rpm.c
then
  AC_MSG_CHECKING(for GNU xgettext)
  xgettext --version 2>&1 | grep 'GNU gettext' >/dev/null 2>&1 || AC_MSG_ERROR([
  *** GNU gettext is required. The latest version
  *** is always available from ftp://ftp.gnu.org/gnu/gettext/.])
  AC_MSG_RESULT(yes)
fi

AC_CHECK_FUNC(setreuid, [], [
    AC_CHECK_LIB(ucb, setreuid, [if echo $LIBS | grep -- -lucb >/dev/null ;then :; else LIBS="$LIBS -lc -lucb" USEUCB=y;fi])
])
AC_CHECK_FUNCS(getuid geteuid mtrace __secure_getenv setregid strerror)


AM_GNU_GETTEXT

POPT_SOURCE_PATH="`pwd`"
AC_DEFINE_UNQUOTED(POPT_SOURCE_PATH, "$POPT_SOURCE_PATH",
	[Full path to popt top_srcdir.])
AC_SUBST(POPT_SOURCE_PATH)

AC_OUTPUT([Doxyfile Makefile intl/Makefile po/Makefile.in])
