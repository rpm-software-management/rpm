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
## License version 1.0 from http://www.opensource.org/licenses/osl.php or
## by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
## 3001 King Ranch Road, Ukiah, CA 95482.
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H
AM_CFLAGS = -Wall -Werror
INCLUDES = -I. -I$(srcdir) -I.. -I$(top_srcdir)/libelf -I$(top_srcdir)/libebl \
	   -I$(top_srcdir)/lib
GCC_INCLUDE = -I$(shell $(CC) -print-file-name=include)
VERSION = 1

LINT = splint

#lib_LIBRARIES = libasm.a
noinst_LIBRARIES = libasm.a libasm_pic.a
noinst_PROGRAMS = $(noinst_LIBRARIES:_pic.a=.so)
#euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = libasm.h

libasm_a_SOURCES = asm_begin.c asm_abort.c asm_end.c asm_error.c \
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

libasm_pic_a_SOURCES =
am_libasm_pic_a_OBJECTS = $(libasm_a_SOURCES:.c=.os)

libasm_so_SOURCES =
libasm.so: libasm_pic.a libasm.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libasm.map,--no-undefined \
	      -Wl,--soname,$@.$(VERSION) \
	      ../libebl/libebl.so ../libelf/libelf.so
	ln -fs $@ $@.$(VERSION)


%.os: %.c %.o
	if $(COMPILE) -c -o $@ -fpic -DPIC -DSHARED -MT $@ -MD -MP \
	  -MF "$(DEPDIR)/$*.Tpo" `test -f '$<' || echo '$(srcdir)/'`$<; \
	then cat "$(DEPDIR)/$*.Tpo" >> "$(DEPDIR)/$*.Po"; \
	     rm -f "$(DEPDIR)/$*.Tpo"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; \
	fi

install: install-am libasm.so
#	$(mkinstalldirs) $(DESTDIR)$(libdir)
#	$(INSTALL_PROGRAM) libasm.so $(DESTDIR)$(libdir)/libasm-$(PACKAGE_VERSION).so
#	ln -fs libasm-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libasm.so.$(VERSION)
#	ln -fs libasm.so.$(VERSION) $(DESTDIR)$(libdir)/libasm.so

uninstall: uninstall-am
#	rm -f $(DESTDIR)$(libdir)/libasm-$(PACKAGE_VERSION).so
#	rm -f $(DESTDIR)$(libdir)/libasm.so.$(VERSION)
#	rm -f $(DESTDIR)$(libdir)/libasm.so
#	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(includedir)/elfutils

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libasm_a_SOURCES))

noinst_HEADERS = libasmP.h symbolhash.h \
	libasm.h

EXTRA_DIST = libasm.map

CLEANFILES = $(am_libasm_pic_a_OBJECTS)
