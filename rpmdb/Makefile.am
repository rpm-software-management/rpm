# Makefile for rpm library.

include $(top_srcdir)/rpm.am

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += @WITH_SQLITE3_INCLUDE@
AM_CPPFLAGS += @WITH_LIBELF_INCLUDE@

check_PROGRAMS =
EXTRA_PROGRAMS =
CLEANFILES =
EXTRA_DIST = gentagtbl.sh

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = \
	dbconfig.c fprint.c fprint.h \
	hdrNVR.c header.c header_internal.c header_internal.h \
	merge.c \
	poptDB.c rpmhash.c rpmhash.h rpmdb.c rpmdb_internal.h \
	tagname.c tagtbl.c
librpmdb_la_SOURCES += db3.c
if SQLITE3
librpmdb_la_SOURCES += sqlite.c
endif
librpmdb_la_LDFLAGS = -release 4.6
librpmdb_la_LIBADD = \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_SQLITE3_LIB@ \
	@WITH_LIBELF_LIB@
if WITH_INTERNAL_DB
librpmdb_la_LIBADD += $(libdb_la)
else
librpmdb_la_LIBADD += @WITH_DB_LIB@
endif

tagtbl.c: Makefile.am $(top_srcdir)/lib/rpmtag.h gentagtbl.sh
	@AWK=${AWK} ${SHELL} gentagtbl.sh $(top_srcdir)/lib/rpmtag.h > $@
BUILT_SOURCES = tagtbl.c

if WITH_INTERNAL_DB
# XXX watchout, $(top_builddir)/db3/libdb.la created by this Makefile may surprise
libdb_la = $(top_builddir)/db3/libdb.la

# XXX grrr, force noinst libdb.la for db3.
# there are more reliable ways to get the BDB version info, just a dirty
# hack for now...
BDBVER = $(shell grep ^LIBVERSION $(top_builddir)/db3/Makefile|cut -f2)
$(libdb_la): $(top_builddir)/db3/libdb-$(BDBVER).la
	sed -e"/^libdir=/s/^.*$$/libdir=''/" \
	  < $(top_builddir)/db3/libdb-$(BDBVER).la > $(libdb_la)
CLEANFILES += $(libdb_la)

rpmlibexec_PROGRAMS =

rpmlibexec_PROGRAMS += rpmdb_archive
rpmdb_archive_SOURCES =
rpmdb_archive_LDADD = \
	$(top_builddir)/db3/db_archive.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_checkpoint
rpmdb_checkpoint_SOURCES =
rpmdb_checkpoint_LDADD = \
	$(top_builddir)/db3/db_checkpoint.o \
	$(top_builddir)/db3/util_log.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_deadlock
rpmdb_deadlock_SOURCES =
rpmdb_deadlock_LDADD = \
	$(top_builddir)/db3/db_deadlock.o \
	$(top_builddir)/db3/util_log.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_dump
rpmdb_dump_SOURCES =
rpmdb_dump_LDADD = \
	$(top_builddir)/db3/db_dump.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_load
rpmdb_load_SOURCES =
rpmdb_load_LDADD = \
	$(top_builddir)/db3/db_load.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_printlog
rpmdb_printlog_SOURCES =
rpmdb_printlog_LDADD = \
	$(top_builddir)/db3/db_printlog.o \
	$(top_builddir)/db3/btree_autop.o \
	$(top_builddir)/db3/crdel_autop.o \
	$(top_builddir)/db3/db_autop.o \
	$(top_builddir)/db3/dbreg_autop.o \
	$(top_builddir)/db3/fileops_autop.o \
	$(top_builddir)/db3/hash_autop.o \
	$(top_builddir)/db3/qam_autop.o \
	$(top_builddir)/db3/rep_autop.o \
	$(top_builddir)/db3/txn_autop.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_recover
rpmdb_recover_SOURCES =
rpmdb_recover_LDADD = \
	$(top_builddir)/db3/db_recover.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_stat
rpmdb_stat_SOURCES =
rpmdb_stat_LDADD = \
	$(top_builddir)/db3/db_stat.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_svc
rpmdb_svc_SOURCES =
rpmdb_svc_LDADD = \
	$(top_builddir)/db3/db_server_proc.o \
	$(top_builddir)/db3/db_server_svc.o \
	$(top_builddir)/db3/db_server_util.o \
	$(top_builddir)/db3/gen_db_server.o \
	$(top_builddir)/db3/util_log.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_upgrade
rpmdb_upgrade_SOURCES =
rpmdb_upgrade_LDADD = \
	$(top_builddir)/db3/db_upgrade.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la

rpmlibexec_PROGRAMS += rpmdb_verify 
rpmdb_verify_SOURCES =
rpmdb_verify_LDADD = \
	$(top_builddir)/db3/db_verify.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpmdb.la
endif

CLEANFILES += $(BUILT_SOURCES)
