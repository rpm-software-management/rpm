LINT = splint
BUILT_SOURCES = listobjs

MAGIC	 = $(pkgdatadir)/magic
AM_CPPFLAGS = -DMAGIC='"$(MAGIC)"'

include_HEADERS = magic.h
noinst_HEADERS = file.h names.h patchlevel.h readelf.h tar.h

#lib_LTLIBRARIES = libmagic.la
usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = libmagic.la

libmagic_la_SOURCES = magic.c apprentice.c softmagic.c ascmagic.c \
	compress.c is_tar.c readelf.c print.c fsmagic.c \
	funcs.c file.h names.h patchlevel.h readelf.h tar.h apptype.c
libmagic_la_LDFLAGS = -version-info 1:0:0

bin_PROGRAMS = file
file_SOURCES = file.c
file_LDADD = libmagic.la

listobjs:
	@echo $(libmagic_la_SOURCES:.c=.lo) > $@
                                                                                
.PHONY: lint
lint:
	$(LINT)  $(DEFS) $(INCLUDES) $(file_SOURCES) $(libmagic_la_SOURCES) $(include_HEADERS) $(noinst_HEADERS)
                                                                                
.PHONY: sources
sources:
	@echo $(libmagic_la_SOURCES:%=file/src/%)

