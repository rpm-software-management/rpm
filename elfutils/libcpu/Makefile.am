## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 2002 Red Hat, Inc.
##
## This program is Open Source software; you can redistribute it and/or
## modify it under the terms of the Open Software License version 1.0 as
## published by the Open Source Initiative.
##
## You should have received a copy of the Open Software License along
## with this program; if not, you may obtain a copy of the Open Software
## License version 1.0 from http://www.opensource.org/license/osl.php or
## by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
## 3001 King Ranch Road, Ukiah, CA 95482.   */
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wshadow
INCLUDES = -I$(srcdir)

LINT = splint

noinst_LTLIBRARIES = libcpu_i386.la

libcpu_i386_la_SOURCES = i386_dis.c

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libcpu_la_SOURCES))
