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
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wshadow
AM_CFLAGS = -Wall -DSRCDIR=\"$(shell cd $(srcdir);pwd)\" -DOBJDIR=\"$(shell pwd)\"
INCLUDES = -I$(srcdir) -I$(srcdir)/../libelf -I$(srcdir)/../libebl -I$(srcdir)/../lib -I$(srcdir)/../libdwarf -I..

YACC = @YACC@ -d
AM_YFLAGS = -pld
AM_LFLAGS = -Pld -olex.yy.c

#bin_PROGRAMS = readelf nm size strip ld elflint
noinst_PROGRAMS = readelf nm size strip ld elflint

noinst_LIBRARIES = libld_elf_i386_pic.a

ld_SOURCES = ld.c ldgeneric.c ldlex.l ldscript.y symbolhash.c sectionhash.c \
	     versionhash.c

noinst_HEADERS = ld.h symbolhash.h sectionhash.h archivehash.h versionhash.h \
		 ldscript.h

EXTRA_DIST = elf32-i386.script $(ld_modules)
ld_modules = i386_ld.c

readelf_LDADD = ../libdwarf/libdwarf.so ../libebl/libebl.so \
		../libelf/libelf.so ../lib/libeu.a
nm_LDADD = ../libdwarf/libdwarf.so ../libebl/libebl.so ../libelf/libelf.so \
	   ../lib/libeu.a
size_LDADD = ../libelf/libelf.so ../lib/libeu.a
strip_LDADD = ../libebl/libebl.so ../libelf/libelf.so ../lib/libeu.a
ld_LDADD = ../libebl/libebl.so ../libelf/libelf.so ../lib/libeu.a
ld_LDFLAGS = -rdynamic
elflint_LDADD  = ../libebl/libebl.so ../libelf/libelf.so ../lib/libeu.a

ldlex.o: ldscript.c
ldscript.h: ldscript.c

libld_elf_i386_pic_a_SOURCES =
am_libld_elf_i386_pic_a_OBJECTS = i386_ld.os

libld_elf_i386.so: libld_elf_i386_pic.a libld_elf_i386.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libld_elf_i386.map,--no-undefined


%.os: %.c %.o
	if $(COMPILE) -c -o $@ -fpic -DPIC -DSHARED -MT $@ -MD -MP \
	  -MF "$(DEPDIR)/$*.Tpo" `test -f '$<' || echo '$(srcdir)/'`$<; \
	then cat "$(DEPDIR)/$*.Tpo" >> "$(DEPDIR)/$*.Po"; \
	     rm -f "$(DEPDIR)/$*.Tpo"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; \
	fi


installcheck-binPROGRAMS: $(bin_PROGRAMS)
	bad=0; pid=$$$$; list="$(bin_PROGRAMS)"; for p in $$list; do \
	  case ' $(AM_INSTALLCHECK_STD_OPTIONS_EXEMPT) ' in \
	   *" $$p "* | *" $(srcdir)/$$p "*) continue;; \
	  esac; \
	  f=`echo "$$p" | \
	     sed 's,^.*/,,;s/$(EXEEXT)$$//;$(transform);s/$$/$(EXEEXT)/'`; \
	  for opt in --help --version; do \
	    if LD_LIBRARY_PATH=$(DESTDIR)$(libdir) \
	       $(DESTDIR)$(bindir)/$$f $$opt > c$${pid}_.out 2> c$${pid}_.err \
	         && test -n "`cat c$${pid}_.out`" \
	         && test -z "`cat c$${pid}_.err`"; then :; \
	    else echo "$$f does not support $$opt" 1>&2; bad=1; fi; \
	  done; \
	done; rm -f c$${pid}_.???; exit $$bad

CLEANFILES = $(ld_modules:.c=.os)
