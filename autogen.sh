#!/bin/sh

export CFLAGS
export LDFLAGS

libtoolize=`which glibtoolize 2>/dev/null`
case $libtoolize in
/*) ;;
*)  libtoolize=`which libtoolize 2>/dev/null`
    case $libtoolize in
    /*) ;;
    *)  libtoolize=libtoolize
    esac
esac

myopts=
if [ X"$@" = X  -a "X`uname -s`" = "XDarwin" -a -d /opt/local ]; then
    export myopts="--prefix=/usr --disable-nls"
    export CPPFLAGS="-I${myprefix}/include"
fi

$libtoolize --copy --force
autopoint --force
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
    ./configure ${myopts} "$@"
fi
