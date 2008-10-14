#!/bin/sh

export CPPFLAGS
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

$libtoolize --copy --force
autopoint --force
aclocal
autoheader
automake -a -c
autoconf

case "$1" in
  "--noconfigure")
    exit 0;
    ;;
  "--rpmconfigure")
    shift
    eval "`rpm --eval %configure`" "$@"
    ;;
  *)
    ./configure "$@"
    ;;
esac
