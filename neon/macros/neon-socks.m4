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

# Following instructions at:
# http://www.socks.nec.com/how2socksify.html

AC_DEFUN([NEON_SOCKS], [

AC_ARG_WITH([--with-socks], 
[  --with-socks[=DIR]      Use SOCKS library ],
[
if test "$withval" != "no"; then

  if test "$withval" != "yes"; then
	LDFLAGS="$LDFLAGS -L$withval/lib"
	CFLAGS="$CFLAGS -I$withval/include"
  fi

  AC_CHECK_HEADERS(sock.h)

  CFLAGS="$CFLAGS -DSOCKS"

  AC_CHECK_LIB(socks5, connect, [NEONLIBS="$NEONLIBS -lsocks5"],
      AC_MSG_ERROR([could not find libsocks5 for SOCKS support]))

fi

])

])

