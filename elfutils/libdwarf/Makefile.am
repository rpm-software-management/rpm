## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
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
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -DDWARF_DEBUG
AM_CFLAGS = -Wall -Werror
INCLUDES = -I. -I$(srcdir) -I$(srcdir)/../libelf -I.. -I$(srcdir)/../lib

#lib_LTLIBRARIES = libdwarf.la
noinst_LTLIBRARIES = libdwarf.la

euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = dwarf.h libdwarf.h

libdwarf_la_SOURCES = dwarf_init.c dwarf_elf_init.c dwarf_get_elf.c \
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

noinst_HEADERS = libdwarfP.h memory-access.h dwarf_abbrev_hash.h \
		dwarf.h libdwarf.h

EXTRA_DIST = AVAILABLE libdwarf.map

libdwarf_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libdwarf.map
libdwarf_la_LIBADD = ../lib/libeu.la ../libelf/libelf.la
libdwarf_la_DEPENDENCIES = $(libdwarf_la_LIBADD) libdwarf.map
