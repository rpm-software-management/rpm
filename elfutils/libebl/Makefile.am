## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 2000, 2001, 2002, 2003 Red Hat, Inc.
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
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall
AM_CFLAGS = -Wall -DOBJDIR=\"$(shell pwd)\" # -Werror

LINT = splint

INCLUDES = -I$(srcdir) -I$(top_srcdir)/libelf -I$(top_srcdir)/lib -I..
VERSION = 1
PACKAGE_VERSION = @PACKAGE_VERSION@

#lib_LIBRARIES = libebl.a
modules = i386 sh mips x86_64 ia64 alpha arm sparc
noinst_LIBRARIES = libebl.a libebl_pic.a \
		   libebl_i386_pic.a libebl_sh_pic.a libebl_mips_pic.a \
		   libebl_x86_64_pic.a libebl_ia64_pic.a libebl_alpha_pic.a \
		   libebl_arm_pic.a libebl_sparc_pic.a
noinst_PROGRAMS = $(noinst_LIBRARIES:_pic.a=.so)

#euincludedir = $(includedir)/elfutils
#euinclude_HEADERS = libebl.h elf-knowledge.h

gen_SOURCES = eblopenbackend.c eblclosebackend.c eblstrtab.c \
	      eblreloctypename.c eblsegmenttypename.c \
	      eblsectiontypename.c eblmachineflagname.c \
	      eblsymboltypename.c ebldynamictagname.c eblsectionname.c \
	      eblobjecttypename.c eblsymbolbindingname.c \
	      eblbackendname.c eblshflagscombine.c eblwstrtab.c \
	      eblgstrtab.c eblosabiname.c eblmachineflagcheck.c \
	      eblreloctypecheck.c ebldynamictagcheck.c \
	      eblcorenotetypename.c eblobjnotetypename.c \
	      eblcorenote.c eblobjnote.c ebldebugscnp.c

libebl_a_SOURCES = $(gen_SOURCES)

libebl_pic_a_SOURCES =
am_libebl_pic_a_OBJECTS = $(gen_SOURCES:.c=.os)

#
# XXX Use --enable-new-dtags as soon as ld.so handles DT_RUNPATH when
# using dlopen correctly.
#	      -Wl,--rpath,\$$ORIGIN/elfutils \
#
libebl_so_SOURCES =
libebl.so: libebl_pic.a libebl.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl.map,--no-undefined \
	      -Wl,--soname,$@.$(VERSION),-z,defs \
	      ../libelf/libelf.so -ldl
	ln -fs $@ $@.$(VERSION)


i386_SRCS = i386_init.c i386_destr.c i386_symbol.c i386_corenote.c
libebl_i386_pic_a_SOURCES =
am_libebl_i386_pic_a_OBJECTS = $(i386_SRCS:.c=.os)

libebl_i386_so_SOURCES =
libebl_i386.so: libebl_i386_pic.a libebl_i386.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_i386.map,--no-undefined \
	      -Wl,-z,defs


sh_SRCS = sh_init.c sh_destr.c sh_symbol.c
libebl_sh_pic_a_SOURCES =
am_libebl_sh_pic_a_OBJECTS = $(sh_SRCS:.c=.os)

libebl_sh_so_SOURCES =
libebl_sh.so: libebl_sh_pic.a libebl_sh.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_sh.map,--no-undefined \
	      -Wl,-z,defs


mips_SRCS = mips_init.c mips_destr.c mips_symbol.c
libebl_mips_pic_a_SOURCES =
am_libebl_mips_pic_a_OBJECTS = $(mips_SRCS:.c=.os)

libebl_mips_so_SOURCES =
libebl_mips.so: libebl_mips_pic.a libebl_mips.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_mips.map,--no-undefined \
	      -Wl,-z,defs


x86_64_SRCS = x86_64_init.c x86_64_destr.c x86_64_symbol.c
libebl_x86_64_pic_a_SOURCES =
am_libebl_x86_64_pic_a_OBJECTS = $(x86_64_SRCS:.c=.os)

libebl_x86_64_so_SOURCES =
libebl_x86_64.so: libebl_x86_64_pic.a libebl_x86_64.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_x86_64.map,--no-undefined \
	      -Wl,-z,defs


ia64_SRCS = ia64_init.c ia64_destr.c ia64_symbol.c
libebl_ia64_pic_a_SOURCES =
am_libebl_ia64_pic_a_OBJECTS = $(ia64_SRCS:.c=.os)

libebl_ia64_so_SOURCES =
libebl_ia64.so: libebl_ia64_pic.a libebl_ia64.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_ia64.map,--no-undefined \
	      -Wl,-z,defs


alpha_SRCS = alpha_init.c alpha_destr.c alpha_symbol.c
libebl_alpha_pic_a_SOURCES =
am_libebl_alpha_pic_a_OBJECTS = $(alpha_SRCS:.c=.os)

libebl_alpha_so_SOURCES =
libebl_alpha.so: libebl_alpha_pic.a libebl_alpha.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_alpha.map,--no-undefined \
	      -Wl,-z,defs


arm_SRCS = arm_init.c arm_destr.c arm_symbol.c
libebl_arm_pic_a_SOURCES =
am_libebl_arm_pic_a_OBJECTS = $(arm_SRCS:.c=.os)

libebl_arm_so_SOURCES =
libebl_arm.so: libebl_arm_pic.a libebl_arm.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_arm.map,--no-undefined \
	      -Wl,-z,defs


sparc_SRCS = sparc_init.c sparc_destr.c sparc_symbol.c
libebl_sparc_pic_a_SOURCES =
am_libebl_sparc_pic_a_OBJECTS = $(sparc_SRCS:.c=.os)

libebl_sparc_so_SOURCES =
libebl_sparc.so: libebl_sparc_pic.a libebl_sparc.map
	$(CC) -shared -o $@ -Wl,--whole-archive,$<,--no-whole-archive \
	      -Wl,--version-script,$(srcdir)/libebl_sparc.map,--no-undefined \
	      -Wl,-z,defs


%.os: %.c %.o
	if $(COMPILE) -c -o $@ -fpic -DPIC -DSHARED -MT $@ -MD -MP \
	  -MF "$(DEPDIR)/$*.Tpo" `test -f '$<' || echo '$(srcdir)/'`$<; \
	then cat "$(DEPDIR)/$*.Tpo" >> "$(DEPDIR)/$*.Po"; \
	     rm -f "$(DEPDIR)/$*.Tpo"; \
	else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; \
	fi

install: install-am libebl.so
#	$(mkinstalldirs) $(DESTDIR)$(libdir)
#	$(INSTALL_PROGRAM) libebl.so $(DESTDIR)$(libdir)/libebl-$(PACKAGE_VERSION).so
#	ln -fs libebl-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/libebl.so.$(VERSION)
#	ln -fs libebl.so.$(VERSION) $(DESTDIR)$(libdir)/libebl.so
#	$(mkinstalldirs) $(DESTDIR)$(libdir)/elfutils
#	for m in $(modules); do \
#	  $(INSTALL_PROGRAM) libebl_$${m}.so $(DESTDIR)$(libdir)/elfutils/libebl_$${m}-$(PACKAGE_VERSION).so; \
#	  ln -fs libebl_$${m}-$(PACKAGE_VERSION).so $(DESTDIR)$(libdir)/elfutils/libebl_$${m}.so; \
#	done

uninstall: uninstall-am
#	rm -f $(DESTDIR)$(libdir)/libebl-$(PACKAGE_VERSION).so
#	rm -f $(DESTDIR)$(libdir)/libebl.so.$(VERSION)
#	rm -f $(DESTDIR)$(libdir)/libebl.so
#	for m in $(modules); do \
#	  rm -f $(DESTDIR)$(libdir)/elfutils/libebl_$${m}-$(PACKAGE_VERSION).so; \
#	  rm -f $(DESTDIR)$(libdir)/elfutils/libebl_$${m}.so; \
#	done
#	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(libdir)/elfutils
#	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(includedir)/elfutils

noinst_HEADERS = libeblP.h $(noinst_LIBRARIES:%_pic.a=%.h) \
	libebl.h elf-knowledge.h

EXTRA_DIST = libebl.map $(noinst_LIBRARIES:%_pic.a=%.map) \
	     $(foreach m,$(modules),$($(m)_SRCS))

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libebl_a_SOURCES))

CLEANFILES = $(am_libebl_pic_a_OBJECTS) \
	     $(foreach m,$(modules),$(am_libebl_$(m)_pic_a_OBJECTS))
