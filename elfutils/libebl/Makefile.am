## Process this file with automake to create Makefile.in
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
## License version 1.0 from http://www.opensource.org/license/osl.php or
## by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
## 3001 King Ranch Road, Ukiah, CA 95482.   */
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall
AM_CFLAGS = -Wall -Werror -DOBJDIR=\"$(shell pwd)\"
INCLUDES = -I$(srcdir) -I$(top_srcdir)/libelf -I$(top_srcdir)/lib -I.. \
	   $(INCLTDL)

LINT = splint

#lib_LTLIBRARIES = libebl.la
#pkglib_LTLIBRARIES = libebl_i386.la libebl_sh.la libebl_mips.la \
#		     libebl_x86_64.la
noinst_LTLIBRARIES = libebl.la \
		     libebl_i386.la libebl_sh.la libebl_mips.la \
		     libebl_x86_64.la

#euincludedir = $(includedir)/elfutils
#euinclude_HEADERS = libebl.h elf-knowledge.h

noinst_HEADERS = libeblP.h $(noinst_LTLIBRARIES:%.la=%.h) \
	libebl.h elf-knowledge.h

EXTRA_DIST = libebl.map $(noinst_LTLIBRARIES:%.la=%.map)

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

libebl_x86_64_la_SOURCES = x86_64_init.c x86_64_destr.c x86_64_symbol.c
libebl_x86_64_la_LDFLAGS = -module \
			   -Wl,--version-script,$(srcdir)/libebl_x86_64.map
libebl_x86_64_la_DEPENDENCIES = libebl_x86_64.map

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libebl_la_SOURCES))

