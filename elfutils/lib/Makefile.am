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
## 3001 King Ranch Road, Ukiah, CA 95482.
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H
AM_CFLAGS = -Wall # -Werror
INCLUDES = -I$(srcdir)/../libelf -I..

noinst_LIBRARIES = libeu.a

libeu_a_SOURCES = xstrdup.c xstrndup.c xmalloc.c next_prime.c crc32.c

noinst_HEADERS = fixedsizehash.h system.h dynamicsizehash.h
EXTRA_DIST = dynamicsizehash.c
