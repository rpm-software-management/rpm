#! /bin/sh
export CFLAGS
export LDFLAGS
libtoolize --force --copy
aclocal
automake -a -c
autoconf
autoheader
