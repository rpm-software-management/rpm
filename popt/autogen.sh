#!/bin/sh

srcdir="`dirname $0`"
test -z "$srcdir" && srcdir=.

THEDIR="`pwd`"

libtoolize=`which glibtoolize 2>/dev/null`
case $libtoolize in
/*) ;;
*)  libtoolize=`which libtoolize 2>/dev/null`
    case $libtoolize in
    /*) ;;
    *)  libtoolize=libtoolize
    esac
esac

cd "$srcdir"
$libtoolize --copy --force
aclocal
autoheader
automake -a -c
autoconf

if [ "$1" = "--noconfigure" ]; then 
    exit 0;
fi

cd "$THEDIR"

if [ X"$@" = X  -a "X`uname -s`" = "XLinux" ]; then
    $srcdir/configure --prefix=/usr "$@"
else
    $srcdir/configure "$@"
fi
