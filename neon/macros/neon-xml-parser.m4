dnl Check for XML parser.
dnl Supports:
dnl  *  libxml (requires version 1.8.3 or later)
dnl  *  expat in -lexpat
dnl  *  expat in -lxmlparse and -lxmltok (as packaged by Debian)
dnl  *  Bundled expat if bundled is passed as arg

dnl Usage:   NEON_XML_PARSER([bundled])

AC_DEFUN([NEON_XML_PARSER],
[

AC_ARG_ENABLE( libxml,
	[  --enable-libxml         force use of libxml ],
	[neon_force_libxml=$enableval],
	[neon_force_libxml=no])

if test "$neon_force_libxml" = "no"; then
dnl Look for expat
AC_CHECK_LIB(expat, XML_Parse,
	neon_expat_libs="-lexpat" neon_found_parser="expat",
	AC_CHECK_LIB(xmlparse, XML_Parse,
		neon_expat_libs="-lxmltok -lxmlparse" neon_found_parser="expat",
		neon_found_parser="no",
		-lxmltok )
	)
else
	neon_found_parser="no"
fi

if test "$neon_found_parser" = "no"; then
	# Have we got libxml 1.8.3 or later?
	AC_CHECK_PROG(XML_CONFIG, xml-config, xml-config)
	if test "$XML_CONFIG" != ""; then
		# Check for recent library
		oLIBS="$LIBS"
		oCFLAGS="$CFLAGS"
		LIBS="$LIBS `$XML_CONFIG --libs`"
		CFLAGS="$CFLAGS `$XML_CONFIG --cflags`"
		AC_CHECK_LIB(xml, xmlCreatePushParserCtxt,
			neon_found_parser="libxml" neon_xml_parser_message="libxml"
			AC_DEFINE(HAVE_LIBXML, 1, [Define if you have libxml]),
			CFLAGS="$oCFLAGS"
			LIBS="$oLIBS"
			AC_WARN([cannot use old libxml (1.8.3 or newer required)])
		)
	fi
fi

if test "$neon_found_parser" = "expat"; then
	# This is crap. Maybe just use AC_CHECK_HEADERS and use the
	# right file by ifdef'ing is best
	AC_CHECK_HEADER(xmlparse.h,
	neon_expat_incs="" neon_found_expatincs="yes",
	AC_CHECK_HEADER(xmltok/xmlparse.h,
	neon_expat_incs="-I/usr/include/xmltok" neon_found_expatincs="yes",
	))
	if test "$neon_found_expatincs" = "yes"; then
		AC_DEFINE(HAVE_EXPAT, 1, [Define if you have expat])
		if test "$neon_expat_incs"; then
			CFLAGS="$CFLAGS $neon_expat_incs"
		fi	
		LIBS="$LIBS $neon_expat_libs"
	else
	       AC_MSG_ERROR(["found expat library but could not find xmlparse.h"])
	fi
	neon_xml_parser_message="expat in $neon_expat_libs"
fi

if test "$neon_found_parser" = "no" ; then
    if test "$1" = "bundled"; then
	    # Use the bundled expat sources
	    LIBOBJS="$LIBOBJS expat/xmltok/xmltok.o expat/xmltok/xmlrole.o expat/xmlparse/xmlparse.o expat/xmlparse/hashtable.o"
	    CFLAGS="$CFLAGS -DXML_DTD -Iexpat/xmlparse -Iexpat/xmltok"
	    AC_MSG_RESULT(using supplied expat XML parser)	
	    AC_DEFINE(HAVE_EXPAT, 1, [Define if you have expat] )
	    neon_xml_parser_message="supplied expat"
    else
	AC_MSG_ERROR([no XML parser was found])
    fi

fi

])