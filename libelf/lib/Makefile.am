AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = sys_elf.h.in

INCLUDES = -I$(topdir) -I. -I$(srcdir)

# generic sources
SRCS1 = begin.c cntl.c end.c errmsg.c errno.c fill.c flag.c getarhdr.c \
	getarsym.c getbase.c getdata.c getident.c getscn.c hash.c kind.c \
	ndxscn.c newdata.c newscn.c next.c nextscn.c rand.c rawdata.c \
	rawfile.c strptr.c update.c version.c checksum.c

# 32-bit sources
SRCS2 = 32.fsize.c 32.getehdr.c 32.getphdr.c 32.getshdr.c 32.newehdr.c \
	32.newphdr.c 32.xlatetof.c

# support
SRCS3 = cook.c data.c input.c assert.c

# nlist
SRCS4 = nlist.c

# opt
SRCS5 = opt.delscn.c

# 64-bit sources
SRCS64 = 64.xlatetof.c gelfehdr.c gelfphdr.c gelfshdr.c gelftrans.c swap64.c

# Versioning sources
SRCS_V = verdef_32_tof.c verdef_32_tom.c verdef_64_tof.c verdef_64_tom.c
HDRS_V = verdef.h verneed.h

CFLAGS = @CFLAGS@ @PICFLAGS@

#lib_LTLIBRARIES = libelf.la
noinst_LTLIBRARIES = libelf.la

libelf_la_SOURCES = \
	$(SRCS1) $(SRCS2) $(SRCS3) $(SRCS4) $(SRCS5) $(SRCS64) $(SRCS_V)
libelf_la_LIBADD = @LIBOBJS@

#pkgincdir = $(includedir)/@PACKAGE@
#pkginc_HEADERS = libelf.h nlist.h gelf.h sys_elf.h

# private header files
noinst_HEADERS = \
	libelf.h nlist.h gelf.h sys_elf.h \
	byteswap.h errors.h ext_types.h private.h elf_repl.h $(HDRS_V)

.PHONY: sources
sources:
	@echo $(libelf_la_SOURCES:%=libelf/%)

.PHONY: lclint
lclint:
	lclint -Dlint $(DEFS) $(INCLUDES) $(SRCS)

t1:	libelf.a t1.o
	$(CC) -o $@ $@.o libelf.a

t2:	libelf.a t2.o
	$(CC) -o $@ $@.o libelf.a
