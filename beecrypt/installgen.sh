#! /bin/sh
libtoolize --force --copy
aclocal
autoheader
automake -a Makefile docs/Makefile gas/Makefile masm/Makefile mwerks/Makefile tests/Makefile
autoconf
