## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 2002 Red Hat, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2 as
## published by the Free Software Foundation.
##
## his program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software Foundation,
## Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H
AM_CFLAGS = -Wall -Werror
INCLUDES = -I. -I$(srcdir) -I.. -I$(top_srcdir)/libelf -I$(top_srcdir)/libebl \
	   -I$(top_srcdir)/lib
GCC_INCLUDE = -I$(shell $(CC) -print-file-name=include)

LINT = splint

#lib_LTLIBRARIES = libasm.la
noinst_LTLIBRARIES = libasm.la

#euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = libasm.h

libasm_la_SOURCES = asm_begin.c asm_abort.c asm_end.c asm_error.c \
		    asm_getelf.c asm_newscn.c asm_newscn_ingrp.c \
		    asm_newsubscn.c asm_newsym.c asm_newcomsym.c \
		    asm_newabssym.c \
		    asm_newscngrp.c asm_scngrp_newsignature.c \
		    asm_fill.c asm_align.c asm_addstrz.c \
		    asm_addint8.c asm_adduint8.c \
		    asm_addint16.c asm_adduint16.c \
		    asm_addint32.c asm_adduint32.c \
		    asm_addint64.c asm_adduint64.c \
		    asm_adduleb128.c asm_addsleb128.c \
		    symbolhash.c

libasm_la_LIBADD = ../libebl/libebl.la ../libelf/libelf.la ../lib/libeu.la
libasm_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libasm.map
libasm_la_DEPENDENCIES = $(libasm_la_LIBADD) libasm.map

noinst_HEADERS = libasmP.h symbolhash.h \
	libasm.h

EXTRA_DIST = libasm.map

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libasm_la_SOURCES))
