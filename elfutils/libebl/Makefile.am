## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits

DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall
AM_CFLAGS = -Wall -Werror -DOBJDIR=\"$(shell pwd)\"
INCLUDES = -I$(srcdir) -I$(top_srcdir)/libelf -I$(top_srcdir)/lib -I.. \
	   $(INCLTDL)

lib_LTLIBRARIES = libebl.la libebl_i386.la libebl_sh.la libebl_mips.la

include_HEADERS = libebl.h elf-knowledge.h

noinst_HEADERS = libeblP.h libebl_i386.h libebl_sh.h libebl_mips.h

EXTRA_DIST = libebl.sym libebl_i386.sym libebl_sh.sym libebl_mips.sym

libebl_la_SOURCES = eblopenbackend.c eblclosebackend.c eblstrtab.c \
		    eblreloctypename.c eblsegmenttypename.c \
		    eblsectiontypename.c eblmachineflagname.c \
		    eblsymboltypename.c ebldynamictagname.c eblsectionname.c \
		    eblobjecttypename.c eblsymbolbindingname.c \
		    eblbackendname.c eblshflagscombine.c eblwstrtab.c \
		    eblgstrtab.c eblosabiname.c eblmachineflagcheck.c \
		    eblreloctypecheck.c ebldynamictagcheck.c
libebl_la_LIBADD = ../lib/libbu.la $(LIBLTDL)
libebl_la_LDFLAGS = -export-symbols $(srcdir)/libebl.sym
libebl_la_DEPENDENCIES = $(libebl_la_LIBADD) libebl.sym

libebl_i386_la_SOURCES = i386_init.c i386_destr.c i386_symbol.c
libebl_i386_la_LDFLAGS = -module -export-symbols $(srcdir)/libebl_i386.sym

libebl_sh_la_SOURCES = sh_init.c sh_destr.c sh_symbol.c
libebl_sh_la_LDFLAGS = -module -export-symbols $(srcdir)/libebl_sh.sym

libebl_mips_la_SOURCES = mips_init.c mips_destr.c mips_symbol.c
libebl_mips_la_LDFLAGS = -module -export-symbols $(srcdir)/libebl_mips.sym
