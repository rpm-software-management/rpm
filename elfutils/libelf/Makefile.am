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
## License version 1.0 from http://www.opensource.org/license/osl.php or
## by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
## 3001 King Ranch Road, Ukiah, CA 95482.   */
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H
AM_CFLAGS = -Wall -Werror
INCLUDES = -I. -I$(srcdir) -I.. -I$(top_srcdir)/lib
GCC_INCLUDE = -I$(shell $(CC) -print-file-name=include)

LINT = splint

#lib_LTLIBRARIES = libelf.la
noinst_LTLIBRARIES = libelf.la

euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = libelf.h gelf.h nlist.h

libelf_la_SOURCES = elf_version.c elf_hash.c elf_error.c elf_fill.c \
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
		    gelf_rawchunk.c gelf_freechunk.c
libelf_la_LIBADD = ../lib/libeu.la
## XXX We might remove the -version-info parameter if all binary compatibility
## XXX issues with the old libelf can be resolved.
libelf_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libelf.map \
		    -version-info 1:0:0
libelf_la_DEPENDENCIES = $(libelf_la_LIBADD) libelf.map

noinst_HEADERS = elf.h abstract.h common.h exttypes.h gelf_xlate.h libelfP.h \
		 version_xlate.h dl-hash.h \
		libelf.h gelf.h nlist.h

EXTRA_DIST = libelf.map

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libelf_la_SOURCES))
