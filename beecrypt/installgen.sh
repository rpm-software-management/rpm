#! /bin/sh
rm -f ltconfig ltmain.sh
aclocal
autoheader
automake -a
autoconf
