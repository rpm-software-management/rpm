
dnl Call these checks when compiling the libneon source package.

AC_DEFUN([LIBNEON_SOURCE_CHECKS],[

AC_CHECK_HEADERS(stdarg.h string.h strings.h sys/time.h regex.h \
	stdlib.h unistd.h limits.h sys/select.h)

AC_REPLACE_FUNCS(strcasecmp)

dnl Check for snprintf
AC_CHECK_FUNC(snprintf,
	AC_DEFINE(HAVE_SNPRINTF, 1, [Define if you have snprintf]),
	LIBOBJS="$LIBOBJS lib/snprintf.o" )

AC_CHECK_FUNC(gethostbyname,
	AC_MSG_RESULT(using libc's gethostbyname),
	AC_CHECK_LIB(nsl,gethostbyname)
	)

])
