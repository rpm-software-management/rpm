# Copyright (C) 1998-2000 Joe Orton.  
# This file is free software; you may copy and/or distribute it with
# or without modifications, as long as this notice is preserved.
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even
# the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.

# The above license applies to THIS FILE ONLY, the library code itself
# may be copied and distributed under the terms of the GNU LGPL, see
# COPYING.LIB for more details

# This file is part of the neon HTTP/WebDAV client library.
# See http://www.webdav.org/neon/ for the latest version. 
# Please send any feedback to <neon@webdav.org>
# Id: neon-warnings.m4,v 1.4 2000/07/27 15:23:24 joe Exp 

# Usage:
#   NEON_WARINGS(default)

AC_DEFUN([NEON_WARNINGS],[

AC_ARG_ENABLE(warnings,
	[  --enable-warnings       enable GCC warnings during build ],
	[with_warnings=$enableval],
	[with_warnings=no])  dnl Defaults to DISABLED

if test "$with_warnings" = "yes"; then
	CFLAGS="$CFLAGS -Wall -ansi-pedantic -Wmissing-declarations -Winline"
	if test -z "$with_ssl" -o "$with_ssl" = "no"; then
		# joe: My OpenSSL ssl.h fails strict prototypes checks
		# left right and center
		CFLAGS="$CFLAGS -Wstrict-prototypes"
	fi
fi

])

