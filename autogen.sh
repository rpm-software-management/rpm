#!/bin/sh

autoheader
autoconf
(cd popt; autoconf)

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ -z "$@" ]; then
    ./configure --prefix=/usr
else
    ./configure "$@"
fi
