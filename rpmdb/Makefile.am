# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

INCLUDES = -I. \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	-I$(top_srcdir)/popt \
	@WITH_LIBELF_INCLUDE@ \
	@INCPATH@

EXTRA_DIST = db3.c

EXTRA_PROGRAMS = tjfn

tjfn_SOURCES = tjfn.c
tjfn_LDFLAGS = -all-static
tjfn_LDADD = librpmdb.la

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = db.h header.h hdrinline.h rpmdb.h rpmhash.h
noinst_HEADERS = fprint.h header_internal.h legacy.h

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = \
	rpmdb_deadlock rpmdb_dump rpmdb_load rpmdb_svc rpmdb_stat rpmdb_verify
noinst_PROGRAMS = \
	rpmdb_archive rpmdb_checkpoint rpmdb_printlog rpmdb_recover \
	rpmdb_upgrade

mylibs = librpmdb.la

LIBS =

# XXX watchout, ../db3/libdb.la created by this Makefile may surprise
libdb_la = $(top_builddir)/$(WITH_DB_SUBDIR)/libdb.la

# XXX grrr, RPM_BUILD_ROOT prevents build pollution if/when -lrpm different
LDFLAGS = -L$(RPM_BUILD_ROOT)$(usrlibdir) -L$(DESTDIR)$(usrlibdir)

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c \
	hdrNVR.c header.c header_internal.c legacy.c merge.c \
	poptDB.c rpmhash.c rpmdb.c \
	tagname.c tagtbl.c
librpmdb_la_LDFLAGS = -release 4.3 \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	@WITH_LIBELF_LIB@
librpmdb_la_LIBADD = $(DBLIBOBJS) $(libdb_la)
librpmdb_la_DEPENDENCIES = $(DBLIBOBJS) $(libdb_la)

# XXX make sure that db.h symlink is generated
dbconfig.c db3.c: db.h

tagtbl.c: Makefile.am $(top_srcdir)/lib/rpmlib.h 
	@echo '/*@-bounds@*/' > $@
	@echo '#include "system.h"' >> $@
	@echo '#include <rpmlib.h>' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'static const struct headerTagTableEntry_s rpmTagTbl[] = {' >> $@
	$(AWK) '/(RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ { if ($$2 == "=") { printf("\t{ \"%s\", %s },\n", $$1, $$3); } else { printf("\t{ \"%s\", %s },\n", $$2, $$3); } }' < $(top_srcdir)/lib/rpmlib.h >> $@
	@echo '	{ NULL, 0 }' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'const struct headerTagTableEntry_s * rpmTagTable = rpmTagTbl;' >> $@
	@echo '' >> $@
	@echo '/*@unchecked@*/' >> $@
	@echo 'const int rpmTagTableSize = sizeof(rpmTagTbl) / sizeof(rpmTagTbl[0]) - 1;' >> $@
	@echo '/*@=bounds@*/' >> $@

db.h:
	@ln -sf ../$(WITH_DB_SUBDIR)/db.h $@

# XXX grrr, force noinst libdb.la for db3.
$(libdb_la):
	sed -e"/^libdir=/s/^.*$$/libdir=''/" < $(top_builddir)/$(WITH_DB_SUBDIR)/libdb-4.2.la > $(libdb_la)

rpmdb_archive_SOURCES =
rpmdb_archive_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_archive.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_checkpoint_SOURCES =
rpmdb_checkpoint_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_checkpoint.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_deadlock_SOURCES =
rpmdb_deadlock_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_deadlock.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_dump_SOURCES =
rpmdb_dump_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_dump.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_load_SOURCES =
rpmdb_load_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_load.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_printlog_SOURCES =
rpmdb_printlog_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_printlog.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_recover_SOURCES =
rpmdb_recover_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_recover.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_stat_SOURCES =
rpmdb_stat_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_stat.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_svc_SOURCES =
rpmdb_svc_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_proc.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_svc.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_util.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/gen_db_server.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	librpmdb.la

rpmdb_upgrade_SOURCES =
rpmdb_upgrade_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_upgrade.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmdb_verify_SOURCES =
rpmdb_verify_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_verify.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

clean-local:
	rm -f *.o db.h $(libdb_la)

BUILT_SOURCES = tagtbl.c

rpmdb.lcd: Makefile.am ${librpmdb_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${librpmdb_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmdb_la_SOURCES:%=rpmdb/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmdb_la_SOURCES)

tdbi: librpmdb.la tdbi.o
	$(LINK) -all-static $@.o $< $(mylibpaths) $(mylibs) $(LIBS)
