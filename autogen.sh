#!/bin/sh

export CFLAGS
export LDFLAGS

LTV="libtoolize (GNU libtool) 1.4"
ACV="Autoconf version 2.13"
AMV="automake (GNU automake) 1.4-p2"
USAGE="
You need to install:
	libtool-1.4
	autoconf-2.13
	automake-1.4-p2
Or edit this script to change the libtool/autoconf/automake versions checked ...
"

[ "`libtoolize --version`" != "$LTV" ] && echo "$USAGE" && exit 1
[ "`autoconf --version`" != "$ACV" ] && echo "$USAGE" && exit 1
[ "`automake --version | head -1 | sed -e 's/1\.4[a-z]/1.4/'`" != "$AMV" ] && echo "$USAGE" && exit 1

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
    if [ -d /usr/share/man ]; then
	mandir=/usr/share/man
	infodir=/usr/share/info
    else
	mandir=/usr/man
	infodir=/usr/info
    fi
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --infodir=${infodir} --mandir=${mandir} "$@"
else
    ./configure "$@"
fi
