#!/bin/sh

LTV="libtoolize (GNU libtool) 1.2d"
ACV="Autoconf version 2.13"
AMV="automake (GNU automake) 1.4"
USAGE="
You need to install:
	libtool-1.2d
	autoconf-2.13
	automake-2.4
"

[ "`libtoolize --version`" != "$LTV" ] && echo "$USAGE" && exit 1
[ "`autoconf --version`" != "$ACV" ] && echo "$USAGE" && exit 1
[ "`automake --version | head -1`" != "$AMV" ] && echo "$USAGE" && exit 1

(cd popt; ./autogen.sh --noconfigure "$@")
libtoolize --copy --force
aclocal
autoheader
automake
autoconf

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    ./configure --disable-shared --prefix=/usr
else
    ./configure --disable-shared "$@"
fi
