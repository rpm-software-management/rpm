## Process this file with automake to create Makefile.in -*-Makefile-*-
## Configure input file for elfutils.
##
## Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -DDWARF_DEBUG
AM_CFLAGS = -Wall # -Werror
INCLUDES = -I. -I$(srcdir) -I$(srcdir)/../libelf -I.. -I$(srcdir)/../lib
VERSION = 1

LINT = splint

#lib_LIBRARIES = libdwarf.a
noinst_LIBRARIES = libdwarf.a libdwarf_pic.a
noinst_PROGRAMS = $(noinst_LIBRARIES:_pic.a=.so)
#include_HEADERS = dwarf.h
#euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = libdwarf.h

libdwarf_a_SOURCES = dwarf_init.c dwarf_elf_init.c dwarf_get_elf.c \
		     dwarf_finish.c \
		     dwarf_next_cu_header.c dwarf_siblingof.c dwarf_child.c \
		     dwarf_offdie.c \
		     dwarf_tag.c dwarf_dieoffset.c dwarf_die_CU_offset.c \
		     dwarf_diename.c dwarf_attrlist.c dwarf_hasattr.c \
		     dwarf_attr.c dwarf_lowpc.c dwarf_highpc.c \
		     dwarf_getconstant.c \
		     dwarf_bytesize.c dwarf_bitsize.c dwarf_bitoffset.c \
		     dwarf_srclang.c dwarf_whatattr.c dwarf_arrayorder.c \
		     dwarf_hasform.c dwarf_whatform.c dwarf_formref.c \
		     dwarf_global_formref.c dwarf_formaddr.c \
		     dwarf_formstring.c dwarf_loclist.c \
		     dwarf_formflag.c dwarf_formudata.c dwarf_formsdata.c \
		     dwarf_formblock.c \
		     dwarf_srclines.c dwarf_srcfiles.c \
		     dwarf_linebeginstatement.c dwarf_lineendsequence.c \
		     dwarf_lineno.c dwarf_lineaddr.c dwarf_lineoff.c \
		     dwarf_linesrc.c dwarf_lineblock.c \
		     dwarf_lineprologueend.c dwarf_lineepiloguebegin.c \
		     dwarf_get_globals.c dwarf_globname.c \
		     dwarf_global_die_offset.c dwarf_global_cu_offset.c \
		     dwarf_global_name_offsets.c \
		     dwarf_error.c dwarf_errno.c dwarf_errmsg.c \
		     dwarf_abbrev_hash.c dwarf_getabbrev.c dwarf_form.c \
		     dwarf_find_macro_value_start.c dwarf_get_str.c \
		     dwarf_get_aranges.c dwarf_get_arange.c \
		     dwarf_get_cu_die_offset.c dwarf_get_arange_info.c \
		     dwarf_get_fde_list_eh.c dwarf_get_cie_of_fde.c \
		     dwarf_get_cie_info.c dwarf_get_fde_instr_bytes.c \
		     dwarf_get_fde_range.c dwarf_get_fde_n.c \
		     dwarf_get_fde_at_pc.c  \
		     dwarf_get_loclist_entry.c \
		     dwarf_get_abbrev.c dwarf_get_abbrev_tag.c \
		     dwarf_get_abbrev_code.c dwarf_get_abbrev_children_flag.c\
		     dwarf_get_abbrev_entry.c \
		     dwarf_get_address_size.c \
		     dwarf_seterrhand.c dwarf_seterrarg.c \
		     dwarf_dealloc.c

libdwarf_pic_a_SOURCES =
am_libdwarf_pic_a_OBJECTS = $(libdwarf_a_SOURCES:.c=.os)

libdwarf_so_SOURCES =
libdwarf.so: libdwarf_pic.a libdwarf.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libdwarf.map,--no-undefined \
	      -Wl,--soname,$@.$(VERSION) \
	      ../libelf/libelf.so
	ln -fs $@ $@.$(VERSION)


%.os: %.c %.o
	if $(COMPILE) -c -o $@ -fpic -DPIC -DSHARED -MT $@ -MD -MP \
	  -MF "$(DEPDIR)/$*.Tpo" `test -f '$<' || echo '$(srcdir)/'`$<; \
	then cat "$(DEPDIR)/$*.Tpo" >> "$(DEPDIR)/$*.Po"; \
	     rm -f "$(DEPDIR)/$*.Tpo"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; \
	fi

install: install-am libdwarf.so
#	$(mkinstalldirs) $(DESTDIR)$(libdir)
#	$(INSTALL_PROGRAM) libdwarf.so $(DESTDIR)$(libdir)/libdwarf-$(PACKAGE_VERSION).so
#	ln -fs libdwarf-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libdwarf.so.$(VERSION)
#	ln -fs libdwarf.so.$(VERSION) $(DESTDIR)$(libdir)/libdwarf.so

uninstall: uninstall-am
#	rm -f $(DESTDIR)$(libdir)/libdwarf-$(PACKAGE_VERSION).so
#	rm -f $(DESTDIR)$(libdir)/libdwarf.so.$(VERSION)
#	rm -f $(DESTDIR)$(libdir)/libdwarf.so
#	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(includedir)/elfutils

noinst_HEADERS = libdwarfP.h memory-access.h dwarf_abbrev_hash.h \
		dwarf.h libdwarf.h

EXTRA_DIST = AVAILABLE libdwarf.map

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libdwarf_a_SOURCES))

CLEANFILES = $(am_libdwarf_pic_a_OBJECTS)
