AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = algorithm.txt ChangeLog Doxyfile FAQ INDEX README \
	zlib.3 zlib.spec \
	amiga/* contrib/* msdos/* nt/* os2/*

BUILT_SOURCES = Doxyfile

noinst_HEADERS = \
	crc32.h deflate.h infblock.h infcodes.h inffast.h inffixed.h \
	inftrees.h infutil.h trees.h zconf.h zlib.h zutil.h

noinst_LTLIBRARIES = libz.la

libz_la_SOURCES = \
	adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
	zutil.c inflate.c infblock.c inftrees.c infutil.c inffast.c

noinst_PROGRAMS = example minigzip

example_SOURCES = example.c
example_LDFLAGS = -L. -lz

minigzip_SOURCES = minigzip.c
minigzip_LDFLAGS = -L. -lz

.PHONY: sources
sources:
	@echo $(libz_la_SOURCES:%=zlib/%)

.PHONY: lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(libz_la_SOURCES)

.PHONY: doxygen
doxygen apidocs: Doxyfile
	rm -rf $@
	mkdir -p $@
	doxygen
