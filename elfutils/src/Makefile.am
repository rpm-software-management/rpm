## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits

DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wshadow
AM_CFLAGS = -Wall -DSRCDIR=\"$(shell cd $(srcdir);pwd)\" -DOBJDIR=\"$(shell pwd)\"
INCLUDES = -I$(srcdir) -I$(srcdir)/../libelf -I$(srcdir)/../libebl -I$(srcdir)/../lib -I$(srcdir)/../libdwarf -I..

YACC = @YACC@ -d
YFLAGS = -pld
LFLAGS = -Pld -olex.yy.c

bin_PROGRAMS = readelf nm size strip ld elflint

lib_LTLIBRARIES = libld_elf_i386.la

ld_SOURCES = ld.c ldgeneric.c ldlex.l ldscript.y symbolhash.c sectionhash.c \
	     versionhash.c

noinst_HEADERS = ld.h symbolhash.h sectionhash.h archivehash.h versionhash.h \
		 ldscript.h

EXTRA_DIST = elf32-i386.script

readelf_LDADD = ../libdwarf/libdwarf.la ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la
nm_LDADD = ../libdwarf/libdwarf.la ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la
size_LDADD = ../libelf/libelf.la ../lib/libbu.la
strip_LDADD = ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la
ld_LDADD = ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la
ld_LDFLAGS = -rdynamic
elflint_LDADD  = ../libebl/libebl.la ../libelf/libelf.la ../lib/libbu.la

ldlex.o: ldscript.c

libld_elf_i386_la_SOURCES = i386_ld.c
libld_elf_i386_la_LDFLAGS = -module
