AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = algorithm.txt ChangeLog Doxyfile FAQ INDEX README \
	zlib.3 zlib.spec \
	amiga/* contrib/* msdos/* nt/* os2/*

noinst_HEADERS = \
	crc32.h deflate.h inffast.h inffixed.h inflate.h \
	inftrees.h trees.h zconf.h zlib.h zutil.h

noinst_LTLIBRARIES = libz.la

libz_la_SOURCES = \
	adler32.c compress.c crc32.c deflate.c gzio.c \
	infback.c inffast.c inflate.c inftrees.c \
	trees.c uncompr.c zutil.c

noinst_PROGRAMS = example minigzip ung

example_SOURCES = example.c
example_LDFLAGS = -L. -lz

minigzip_SOURCES = minigzip.c
minigzip_LDFLAGS = -L. -lz

ung_SOURCES = ung.c
ung_LDFLAGS = -L. -lz

#BUILT_SOURCES = Doxyfile zlib.lcd

zlib.lcd: Makefile.am ${libz_la_SOURCES} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${libz_la_SOURCES} -dump $@ 2>/dev/null

.PHONY: sources
sources:
	@echo $(libz_la_SOURCES:%=zlib/%)

.PHONY: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(libz_la_SOURCES)

.PHONY: doxygen
doxygen apidocs: Doxyfile
	rm -rf $@
	mkdir -p $@
	doxygen
