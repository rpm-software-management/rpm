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
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall
AM_CFLAGS = -Wall -Werror -DOBJDIR=\"$(shell pwd)\"
INCLUDES = -I$(srcdir) -I$(top_srcdir)/libelf -I$(top_srcdir)/lib -I.. \
	   $(INCLTDL)

LINT = splint

#lib_LTLIBRARIES = libebl.la
#pkglib_LTLIBRARIES = libebl_i386.la libebl_sh.la libebl_mips.la
noinst_LTLIBRARIES = libebl.la \
		libebl_i386.la libebl_sh.la libebl_mips.la

euincludedir = ${includedir}/elfutils
#euinclude_HEADERS = libebl.h elf-knowledge.h

noinst_HEADERS = libeblP.h libebl_i386.h libebl_sh.h libebl_mips.h \
		libebl.h elf-knowledge.h

EXTRA_DIST = libebl.map libebl_i386.map libebl_sh.map libebl_mips.map

libebl_la_SOURCES = eblopenbackend.c eblclosebackend.c eblstrtab.c \
		    eblreloctypename.c eblsegmenttypename.c \
		    eblsectiontypename.c eblmachineflagname.c \
		    eblsymboltypename.c ebldynamictagname.c eblsectionname.c \
		    eblobjecttypename.c eblsymbolbindingname.c \
		    eblbackendname.c eblshflagscombine.c eblwstrtab.c \
		    eblgstrtab.c eblosabiname.c eblmachineflagcheck.c \
		    eblreloctypecheck.c ebldynamictagcheck.c \
		    eblcorenotetypename.c eblobjnotetypename.c \
		    eblcorenote.c eblobjnote.c
libebl_la_LIBADD = ../lib/libeu.la $(LIBLTDL)
libebl_la_LDFLAGS = -Wl,--version-script,$(srcdir)/libebl.map
libebl_la_DEPENDENCIES = $(libebl_la_LIBADD) libebl.map

libebl_i386_la_SOURCES = i386_init.c i386_destr.c i386_symbol.c i386_corenote.c
libebl_i386_la_LDFLAGS = -module -Wl,--version-script,$(srcdir)/libebl_i386.map
libebl_i386_la_DEPENDENCIES = libebl_i386.map

libebl_sh_la_SOURCES = sh_init.c sh_destr.c sh_symbol.c
libebl_sh_la_LDFLAGS = -module -Wl,--version-script,$(srcdir)/libebl_sh.map
libebl_sh_la_DEPENDENCIES = libebl_sh.map

libebl_mips_la_SOURCES = mips_init.c mips_destr.c mips_symbol.c
libebl_mips_la_LDFLAGS = -module -Wl,--version-script,$(srcdir)/libebl_mips.map
libebl_mips_la_DEPENDENCIES = libebl_mips.map

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) -f $(top_srcdir)/splint.rc \
		$(addprefix $(srcdir)/,$(libebl_la_SOURCES))
