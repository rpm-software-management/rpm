#!/bin/sh

autoheader
autoconf
#echo timestamp > stamp-h.in
#(cd popt; autoconf; echo timestamp > stamp-h.in)

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    ./configure --prefix=/usr --disable-shared
else
    ./configure "$@"
fi
