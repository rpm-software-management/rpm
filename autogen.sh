#!/bin/sh

export CFLAGS
export LDFLAGS

LTV="libtoolize (GNU libtool) 1.5.6"
ACV="autoconf (GNU Autoconf) 2.59"
AMV="automake (GNU automake) 1.9"
USAGE="
This script documents the versions of the tools I'm using to build rpm:
	libtool-1.5.6
	autoconf-2.59
	automake-1.9
Simply edit this script to change the libtool/autoconf/automake versions
checked if you need to, as rpm should build (and has built) with all
recent versions of libtool/autoconf/automake.
"

[ "`libtoolize --version | head -1`" != "$LTV" ] && echo "$USAGE" && exit 1
[ "`autoconf --version | head -1`" != "$ACV" ] && echo "$USAGE" && exit 1
[ "`automake --version | head -1 | sed -e 's/1\.4[a-z]/1.4/'`" != "$AMV" ] && echo "$USAGE" && exit 1

if [ -d popt ]; then
    (echo "--- popt"; cd popt; ./autogen.sh --noconfigure "$@")
fi
if [ -d zlib ]; then
    (echo "--- zlib"; cd zlib; ./autogen.sh --noconfigure "$@")
fi
if [ -d beecrypt ]; then
    (echo "--- beecrypt"; cd beecrypt; ./autogen.sh --noconfigure "$@")
fi
if [ -d elfutils ]; then
    (echo "--- elfutils"; cd elfutils; ./autogen.sh --noconfigure "$@")
fi
if [ -d file ]; then
    (echo "--- file"; cd file; ./autogen.sh --noconfigure "$@")
fi

echo "--- rpm"
libtoolize --copy --force
aclocal
autoheader
automake -a -c
autoconf

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    if [ -d /usr/share/man ]; then
	mandir=/usr/share/man
	infodir=/usr/share/info
    else
	mandir=/usr/man
	infodir=/usr/info
    fi
    if [ -d /usr/lib/nptl ]; then
	enable_posixmutexes="--enable-posixmutexes"
    else
	enable_posixmutexes=
    fi
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --infodir=${infodir} --mandir=${mandir} ${enable_posixmutexes} "$@"
else
    ./configure "$@"
fi
