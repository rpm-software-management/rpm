## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits

DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H
AM_CFLAGS = -Wall -Werror
INCLUDES = -I. -I$(srcdir) -I.. -I$(top_srcdir)/libelf -I$(top_srcdir)/libebl \
	   -I$(top_srcdir)/lib
GCC_INCLUDE = -I$(shell $(CC) -print-file-name=include)

LINT = splint

lib_LTLIBRARIES = libasm.la

include_HEADERS = libasm.h

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
		    symbolhash.c

libasm_la_LIBADD = ../libebl/libebl.la ../libelf/libelf.la
libasm_la_LDFLAGS = -export-symbols $(srcdir)/libasm.sym
libasm_la_DEPENDENCIES = $(libasm_la_LIBADD) libasm.sym

noinst_HEADERS = libasmP.h symbolhash.h

EXTRA_DIST = libasm.sym

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) -f $(top_srcdir)/splint.rc \
		$(addprefix $(srcdir)/,$(libelf_la_SOURCES))
