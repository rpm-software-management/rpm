
AC_DEFUN([NEON_LIBRARY],[

AC_ARG_WITH( neon,
	[  --with-neon	          Specify location of neon library ],
	[neon_loc="$withval"])

if test "$1" = "bundle"; then
	AC_ARG_WITH( included-neon,
		[  --with-included-neon   Force use of included neon library ],
		[neon_included="$withval"],
		[neon_included="no"])
fi

AC_MSG_CHECKING(for neon location)

if test "$neon_included" != "yes"; then
    # Look for neon

    if test -z "$neon_loc"; then
    # Look in standard places
	for d in /usr /usr/local; do
	    if test -x $d/bin/neon-config; then
		neon_loc=$d
	    fi
	done
    fi

    if test -x $neon_loc/bin/neon-config; then
	# Couldn't find it!
	AC_MSG_RESULT(found in $neon_loc)
	NEON_CONFIG=$neon_loc/bin/neon-config
	CFLAGS="$CFLAGS `$NEON_CONFIG --cflags`"
	LIBS="$LIBS `$NEON_CONFIG --libs`"
    else
	if test "$1" = "bundle"; then
	    neon_included="yes"
	else
	    AC_MSG_ERROR(could not find neon)
	fi
    fi
fi

if test "$neon_included" = "yes"; then
    AC_MSG_RESULT(using supplied)
    NEON_XML_PARSER
    CFLAGS="$CFLAGS -Ilibneon"
    LIBNEON_SOURCE_CHECKS
    LIBOBJS="$LIBOBJS \$(NEONOBJS)"
fi

])

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

NEON_SSL

])
