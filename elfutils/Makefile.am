## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 1996-2001, 2002 Red Hat, Inc.
##
## This program is Open Source software; you can redistribute it and/or
## modify it under the terms of the Open Software License version 1.0 as
## published by the Open Source Initiative.
##
## You should have received a copy of the Open Software License along
## with this program; if not, you may obtain a copy of the Open Software
## License version 1.0 from http://www.opensource.org/licenses/osl.php or
## by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
## 3001 King Ranch Road, Ukiah, CA 95482.   */
##

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 doc lib libelf libebl libdwarf libdw libcpu libasm src \
	  libelf-po libebl-po libdwarf-po libasm-po po tests

EXTRA_DIST = config.rpath splint.rc elfutils.spec GPG-KEY NOTES

distcheck-hook:
	chmod -R u+w $(distdir)

# Tell version 3.79 and up of GNU make to not build goals in this
# directory in parallel.
.NOTPARALLEL:
