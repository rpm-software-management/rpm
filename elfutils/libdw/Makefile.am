## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 2002, 2003 Red Hat, Inc.
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
AM_CFLAGS = -Wall -Wshadow # -Werror
INCLUDES = -I. -I$(srcdir) -I$(srcdir)/../libelf -I.. -I$(srcdir)/../lib
VERSION = 1

LINT = splint

#lib_LIBRARIES = libdw.a
noinst_LIBRARIES = libdw.a libdw_pic.a
noinst_PROGRAMS = $(noinst_LIBRARIES:_pic.a=.so)

#include_HEADERS = dwarf.h
#euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = libdw.h

libdw_a_SOURCES = dwarf_begin.c dwarf_begin_elf.c dwarf_end.c dwarf_get_elf.c \
		  dwarf_get_pubnames.c dwarf_get_abbrev.c dwarf_tag.c \
		  dwarf_error.c dwarf_nextcu.c dwarf_diename.c dwarf_offdie.c \
		  dwarf_attr.c dwarf_formstring.c dwarf_abbrev_hash.c \
		  dwarf_child.c dwarf_has_children.c dwarf_formaddr.c \
		  dwarf_formudata.c dwarf_formsdata.c dwarf_lowpc.c \
		  dwarf_highpc.c dwarf_formref.c dwarf_siblingof.c \
		  libdw_findcu.c libdw_form.c libdw_alloc.c


libdw_pic_a_SOURCES =
am_libdw_pic_a_OBJECTS = $(libdw_a_SOURCES:.c=.os)

libdw_so_SOURCES =
libdw.so: libdw_pic.a $(srcdir)/libdw.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libdw.map,--no-undefined \
	      -Wl,--soname,$@.$(VERSION),-z,defs \
	      ../libelf/libelf.so
	ln -fs $@ $@.$(VERSION)


%.os: %.c %.o
	if $(COMPILE) -c -o $@ -fpic -DPIC -DSHARED -MT $@ -MD -MP \
	   -MF "$(DEPDIR)/$*.Tpo" `test -f '$<' || echo '$(srcdir)/'`$<; \
	then cat "$(DEPDIR)/$*.Tpo" >> "$(DEPDIR)/$*.Po"; \
	     rm -f "$(DEPDIR)/$*.Tpo"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; \
	fi

install: install-am libdw.so
#	$(mkinstalldirs) $(DESTDIR)$(libdir)
#	$(INSTALL_PROGRAM) libdw.so $(DESTDIR)$(libdir)/libdw-$(PACKAGE_VERSION).so
#	ln -fs libdw-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libdw.so.$(VERSION)
#	ln -fs libdw.so.$(VERSION) $(DESTDIR)$(libdir)/libdw.so

uninstall: uninstall-am
#	rm -f $(DESTDIR)$(libdir)/libdw-$(PACKAGE_VERSION).so
#	rm -f $(DESTDIR)$(libdir)/libdw.so.$(VERSION)
#	rm -f $(DESTDIR)$(libdir)/libdw.so
#	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(includedir)/elfutils

noinst_HEADERS = libdwP.h memory-access.h \
	dwarf.h libdw.h

EXTRA_DIST = libdw.map

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libdw_a_SOURCES))

CLEANFILES = $(am_libdw_pic_a_OBJECTS)
