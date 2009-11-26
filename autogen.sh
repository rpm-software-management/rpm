#!/bin/sh

export CPPFLAGS
export CFLAGS
export LDFLAGS

autoreconf -i

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
