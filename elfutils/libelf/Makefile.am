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
INCLUDES = -I$(srcdir) -I$(top_srcdir)/lib -I..
GCC_INCLUDE = -I$(shell $(CC) -print-file-name=include)
VERSION = 1
PACKAGE_VERSION = @PACKAGE_VERSION@

LINT = splint

#lib_LIBRARIES = libelf.a
noinst_LIBRARIES = libelf.a libelf_pic.a
noinst_PROGRAMS = $(noinst_LIBRARIES:_pic.a=.so)
#include_HEADERS = libelf.h gelf.h nlist.h

libelf_a_SOURCES = elf_version.c elf_hash.c elf_error.c elf_fill.c \
		   elf_begin.c elf_next.c elf_rand.c elf_end.c elf_kind.c \
		   gelf_getclass.c elf_getbase.c elf_getident.c \
		   elf32_fsize.c elf64_fsize.c gelf_fsize.c \
		   elf32_xlatetof.c elf32_xlatetom.c elf64_xlatetof.c \
		   elf64_xlatetom.c gelf_xlate.c \
		   elf32_getehdr.c elf64_getehdr.c gelf_getehdr.c \
		   elf32_newehdr.c elf64_newehdr.c gelf_newehdr.c \
		   gelf_update_ehdr.c \
		   elf32_getphdr.c elf64_getphdr.c gelf_getphdr.c \
		   elf32_newphdr.c elf64_newphdr.c gelf_newphdr.c \
		   gelf_update_phdr.c \
		   elf_getarhdr.c elf_getarsym.c \
		   elf_rawfile.c elf_readall.c elf_cntl.c \
		   elf_getscn.c elf_nextscn.c elf_ndxscn.c elf_newscn.c \
		   elf32_getshdr.c elf64_getshdr.c gelf_getshdr.c \
		   gelf_update_shdr.c \
		   elf_strptr.c elf_rawdata.c elf_getdata.c elf_newdata.c \
		   elf_flagelf.c elf_flagehdr.c elf_flagphdr.c elf_flagscn.c \
		   elf_flagshdr.c elf_flagdata.c elf_memory.c \
		   elf_update.c elf32_updatenull.c elf64_updatenull.c \
		   elf32_updatefile.c elf64_updatefile.c \
		   gelf_getsym.c gelf_update_sym.c \
		   gelf_getversym.c gelf_getverneed.c gelf_getvernaux.c \
		   gelf_getverdef.c gelf_getverdaux.c \
		   gelf_getrel.c gelf_getrela.c \
		   gelf_update_rel.c gelf_update_rela.c \
		   gelf_getdyn.c gelf_update_dyn.c \
		   gelf_getmove.c gelf_update_move.c \
		   gelf_getsyminfo.c gelf_update_syminfo.c \
		   gelf_xlatetof.c gelf_xlatetom.c \
		   nlist.c \
		   gelf_getsymshndx.c gelf_update_symshndx.c \
		   gelf_update_versym.c gelf_update_verneed.c \
		   gelf_update_vernaux.c gelf_update_verdef.c \
		   gelf_update_verdaux.c \
		   elf_getshnum.c elf_getshstrndx.c \
		   gelf_checksum.c elf32_checksum.c elf64_checksum.c \
		   gelf_rawchunk.c gelf_freechunk.c \
		   libelf_crc32.c libelf_next_prime.c

libelf_pic_a_SOURCES =
am_libelf_pic_a_OBJECTS = $(libelf_a_SOURCES:.c=.os)

libelf_so_SOURCES =
libelf.so: libelf_pic.a libelf.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libelf.map,--no-undefined \
	      -Wl,--soname,$@.$(VERSION)
	ln -fs $@ $@.$(VERSION)

%.os: %.c %.o
	if $(COMPILE) -c -o $@ -fpic -DPIC -DSHARED -MT $@ -MD -MP \
	  -MF "$(DEPDIR)/$*.Tpo" `test -f '$<' || echo '$(srcdir)/'`$<; \
	then cat "$(DEPDIR)/$*.Tpo" >> "$(DEPDIR)/$*.Po"; \
	     rm -f "$(DEPDIR)/$*.Tpo"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; \
	fi

install: install-am libelf.so
#	$(mkinstalldirs) $(DESTDIR)$(libdir)
#	$(INSTALL_PROGRAM) libelf.so $(DESTDIR)$(libdir)/libelf-$(PACKAGE_VERSION).so
#	ln -fs libelf-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libelf.so.$(VERSION)
#	ln -fs libelf.so.$(VERSION) $(DESTDIR)$(libdir)/libelf.so

uninstall: uninstall-am
#	rm -f $(DESTDIR)$(libdir)/libelf-$(PACKAGE_VERSION).so
#	rm -f $(DESTDIR)$(libdir)/libelf.so.$(VERSION)
#	rm -f $(DESTDIR)$(libdir)/libelf.so

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libelf_a_SOURCES))

noinst_HEADERS = elf.h abstract.h common.h exttypes.h gelf_xlate.h libelfP.h \
		version_xlate.h dl-hash.h \
		libelf.h gelf.h nlist.h

EXTRA_DIST = libelf.map

CLEANFILES = $(am_libelf_pic_a_OBJECTS)
