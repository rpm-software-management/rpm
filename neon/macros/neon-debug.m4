# Copyright (C) 1998-2001 Joe Orton <joe@manyfish.co.uk>
#
# This file is free software; you may copy and/or distribute it with
# or without modifications, as long as this notice is preserved.
# This software is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even
# the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.

# The above license applies to THIS FILE ONLY, the neon library code
# itself may be copied and distributed under the terms of the GNU
# LGPL, see COPYING.LIB for more details

# This file is part of the neon HTTP/WebDAV client library.
# See http://www.webdav.org/neon/ for the latest version. 
# Please send any feedback to <neon@webdav.org>

# Adds an --disable-debug argument to configure to allow disabling
# debugging messages.
#
# Usage:
#   NEON_WARNINGS([actions-if-debug-enabled], [actions-if-debug-disable])
#

AC_DEFUN([NEON_DEBUG], [

AC_ARG_ENABLE(debug,
	[  --disable-debug         disable runtime debugging messages ],
	[with_debug=$enableval],
	[with_debug=yes])  dnl Defaults to ENABLED

if test "$with_debug" = "yes"; then
	AC_DEFINE(NE_DEBUGGING, 1, [Define to enable debugging])
	:
	$1
else
	:
	$2
fi

])
