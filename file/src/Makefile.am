LINT = splint
BUILT_SOURCES = listobjs

EXTRA_DIST = test.c

pkglibdir =  @prefix@/lib/rpm
MAGIC    = $(pkglibdir)/magic

AM_CPPFLAGS = -DMAGIC='"$(MAGIC)"'

include_HEADERS = # magic.h
noinst_HEADERS = file.h magic.h names.h patchlevel.h readelf.h tar.h

#lib_LTLIBRARIES = libmagic.la
#usrlibdir = $(libdir)@MARK64@
#usrlib_LTLIBRARIES = libmagic.la
noinst_LTLIBRARIES = libmagic.la

libmagic_la_SOURCES = magic.c apprentice.c softmagic.c ascmagic.c \
	compress.c is_tar.c readelf.c print.c fsmagic.c \
	funcs.c apptype.c
libmagic_la_LDFLAGS = -version-info 1:0:0

noinst_PROGRAMS = file
file_SOURCES = file.c
file_LDFLAGS = # -all-static
file_LDADD = libmagic.la

listobjs:
	@echo $(libmagic_la_SOURCES:.c=.lo) > $@

.PHONY: lint
lint:
	$(LINT)  $(DEFS) $(INCLUDES) $(file_SOURCES) $(libmagic_la_SOURCES) $(include_HEADERS) $(noinst_HEADERS)
                                                                                 
.PHONY: sources
sources:
	@echo $(libmagic_la_SOURCES:%=file/src/%)
