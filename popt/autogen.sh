#!/bin/sh

libtoolize --copy
autoheader
autoconf

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    ./configure --prefix=/usr --disable-shared
else
    ./configure "$@"
fi
