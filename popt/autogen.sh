#!/bin/sh

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
