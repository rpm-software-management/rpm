AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = LEGAL.NOTICE MAINT Makefile.std magic2mime magic.mime \
	Localstuff Header file.man

BUILT_SOURCES = listobjs $(man_MANS)

noinst_HEADERS = debug.h names.h patchlevel.h readelf.h system.h tar.h \
		file.h

#pkgincdir =	@includedir@/fmagic
#pkginc_HEADERS = file.h

#usrlibdir = $(libdir)@MARK64@
#usrlib_LTLIBRARIES = libfmagic.la

noinst_LTLIBRARIES = libfmagic.la

libfmagic_la_SOURCES = \
	apprentice.c ascmagic.c fsmagic.c compress.c \
	print.c readelf.c softmagic.c

#bin_PROGRAMS =	file
noinst_PROGRAMS =	file

file_SOURCES =	file.c
file_LDFLAGS =	-all-static
file_LDADD =	libfmagic.la

#man_MANS = file.1 $(man_MAGIC)
noinst_MANS = file.1

pkglibdir =  @prefix@/lib/rpm
MAGIC	 = $(pkglibdir)/magic
CPPFLAGS = -DMAGIC='"$(MAGIC)"'

fsect = @fsect@

file.1:	Makefile file.man
	@rm -f $@
	sed -e s@__CSECTION__@1@g \
	    -e s@__FSECTION__@${fsect}@g \
	    -e s@__VERSION__@${VERSION}@g \
	    -e s@__MAGIC__@${MAGIC}@g $(srcdir)/file.man > $@

listobjs:
	@echo $(libfmagic_la_SOURCES:.c=.lo) > $@

.PHONY: lint
lint:
	$(LINT)  $(DEFS) $(INCLUDES) $(file_SOURCES) $(libfmagic_la_SOURCES) $(pkginc_HEADERS) $(noinst_HEADERS)

.PHONY: sources
sources:
	@echo $(libfmagic_la_SOURCES:%=file/%)

