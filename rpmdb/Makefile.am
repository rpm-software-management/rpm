# Makefile for rpm library.

include $(top_srcdir)/rpm.am

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc \
	@WITH_SQLITE3_INCLUDE@ \
	@WITH_LIBELF_INCLUDE@

EXTRA_PROGRAMS =
check_PROGRAMS =
CLEANFILES =

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = header.h hdrinline.h rpmdb.h 

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c fprint.h \
	hdrNVR.c header.c header_internal.c header_internal.h \
	legacy.c legacy.h \
	merge.c \
	poptDB.c rpmhash.c rpmhash.h rpmdb.c rpmdb_internal.h \
	tagname.c tagtbl.c
librpmdb_la_SOURCES += db3.c
if SQLITE3
librpmdb_la_SOURCES += sqlite.c
endif
librpmdb_la_LDFLAGS = -release 4.4
librpmdb_la_LIBADD = \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_BEECRYPT_LIB@ \
	@WITH_POPT_LIB@ \
	@WITH_SQLITE3_LIB@ \
	@WITH_LIBELF_LIB@
if WITH_INTERNAL_DB
librpmdb_la_LIBADD += $(libdb_la)
else
librpmdb_la_LIBADD += @WITH_DB_LIB@
endif

tagtbl.c: Makefile.am $(top_srcdir)/lib/rpmlib.h 
	@echo '#include "system.h"' > $@
	@echo '#include <rpmlib.h>' >> $@
	@echo '#include "debug.h"' >> $@
	@echo '' >> $@
	@echo 'static const struct headerTagTableEntry_s rpmTagTbl[] = {' >> $@
	${AWK} '/(RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ {tt = "NULL"; ta = "ANY"; if ($$5 == "c") {tt = "CHAR"; ta = "SCALAR"} if ($$5 == "c[]") {tt = "CHAR"; ta = "ARRAY"} if ($$5 == "h") {tt = "INT16"; ta = "SCALAR"} if ($$5 == "h[]") {tt = "INT16"; ta = "ARRAY"} if ($$5 == "i") {tt = "INT32"; ta = "SCALAR"} if ($$5 == "i[]") {tt = "INT32"; ta = "ARRAY"} if ($$5 == "l") {tt = "INT64"; ta = "SCALAR"} if ($$5 == "l[]") {tt = "INT64"; ta = "ARRAY"} if ($$5 == "s") {tt = "STRING"; ta = "SCALAR"} if ($$5 == "s[]") {tt = "STRING_ARRAY"; ta = "ARRAY"} if ($$5 == "s{}") {tt = "I18NSTRING"; ta = "SCALAR"} if ($$5 == "x") {tt = "BIN"; ta = "SCALAR"} if ($$2 == "=") { printf("\t{ \"%s\",	%s	RPM_%s_TYPE + RPM_%s_RETURN_TYPE },\n", $$1, $$3, tt, ta) } else { printf("\t{ \"%s\",	%s,	RPM_%s_TYPE + RPM_%s_RETURN_TYPE  },\n", $$2, $$3, tt, ta) } }' < ${top_srcdir}/lib/rpmlib.h | sort >> $@
	@echo '	{ NULL, 0, 0 }' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo 'const struct headerTagTableEntry_s * rpmTagTable = rpmTagTbl;' >> $@
	@echo '' >> $@
	@echo 'const int rpmTagTableSize = sizeof(rpmTagTbl) / sizeof(rpmTagTbl[0]) - 1;' >> $@
BUILT_SOURCES = tagtbl.c

if WITH_INTERNAL_DB
librpmdb_la_SOURCES += db.h
db.h:	../$(WITH_DB_SUBDIR)/db.h
	$(LN_S) ../$(WITH_DB_SUBDIR)/db.h $@
BUILT_SOURCES += db.h

# XXX watchout, ../db3/libdb.la created by this Makefile may surprise
libdb_la = $(top_builddir)/$(WITH_DB_SUBDIR)/libdb.la

# XXX grrr, force noinst libdb.la for db3.
$(libdb_la): $(top_builddir)/$(WITH_DB_SUBDIR)/libdb-4.6.la
	sed -e"/^libdir=/s/^.*$$/libdir=''/" \
	  < $(top_builddir)/$(WITH_DB_SUBDIR)/libdb-4.6.la > $(libdb_la)
CLEANFILES += $(libdb_la)

rpmlibexec_PROGRAMS =

rpmlibexec_PROGRAMS += rpmdb_archive
rpmdb_archive_SOURCES =
rpmdb_archive_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_archive.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_checkpoint
rpmdb_checkpoint_SOURCES =
rpmdb_checkpoint_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_checkpoint.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_deadlock
rpmdb_deadlock_SOURCES =
rpmdb_deadlock_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_deadlock.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_dump
rpmdb_dump_SOURCES =
rpmdb_dump_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_dump.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_load
rpmdb_load_SOURCES =
rpmdb_load_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_load.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_printlog
rpmdb_printlog_SOURCES =
rpmdb_printlog_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_printlog.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/btree_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/crdel_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/dbreg_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/fileops_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/hash_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/qam_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/rep_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/txn_autop.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_recover
rpmdb_recover_SOURCES =
rpmdb_recover_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_recover.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_stat
rpmdb_stat_SOURCES =
rpmdb_stat_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_stat.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_svc
rpmdb_svc_SOURCES =
rpmdb_svc_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_proc.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_svc.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_server_util.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/gen_db_server.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_log.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_upgrade
rpmdb_upgrade_SOURCES =
rpmdb_upgrade_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_upgrade.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_verify 
rpmdb_verify_SOURCES =
rpmdb_verify_LDADD = \
	$(top_builddir)/$(WITH_DB_SUBDIR)/db_verify.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_cache.o \
	$(top_builddir)/$(WITH_DB_SUBDIR)/util_sig.o \
	librpmdb.la
endif

check_PROGRAMS += tdbi
tdbi_SOURCES = tdbi.c
tdbi_LDADD = librpmdb.la

check_PROGRAMS += tjfn
tjfn_SOURCES = tjfn.c
tjfn_LDADD = librpmdb.la @WITH_BEECRYPT_LIB@

EXTRA_PROGRAMS += tsql
tsql_SOURCES = tsql.c
tsql_LDADD = librpmdb.la

CLEANFILES += $(BUILT_SOURCES)
