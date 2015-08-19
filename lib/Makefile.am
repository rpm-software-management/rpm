# Makefile for rpm library.

include $(top_srcdir)/rpm.am

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_NSS_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\""
AM_CPPFLAGS += -DLOCALSTATEDIR="\"$(localstatedir)\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"rpmpopt-${VERSION}\""
AM_CPPFLAGS += -DLIBRPMALIAS_EXECPATH="\"$(bindir)\""

usrlibdir = $(libdir)

check_PROGRAMS =
CLEANFILES =
EXTRA_DIST = gentagtbl.sh tagtbl.C rpmhash.C rpmhash.H
EXTRA_PROGRAMS =

usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	backend/db3.c backend/dbi.c backend/dbi.h \
	backend/dbiset.c backend/dbiset.h \
	headerutil.c header.c headerfmt.c header_internal.h \
	rpmdb.c rpmdb_internal.h \
	fprint.c fprint.h tagname.c rpmtd.c \
	cpio.c cpio.h depends.c order.c formats.c tagexts.c fsm.c fsm.h \
	manifest.c manifest.h package.c \
	poptALL.c poptI.c poptQV.c psm.c query.c \
	rpmal.c rpmal.h rpmchecksig.c rpmds.c rpmds_internal.h \
	rpmfi.c rpmfi_internal.h \
	rpmgi.h rpmgi.c rpminstall.c rpmts_internal.h \
	rpmlead.c rpmlead.h rpmps.c rpmprob.c rpmrc.c \
	rpmte.c rpmte_internal.h rpmts.c rpmfs.h rpmfs.c \
	rpmvercmp.c signature.c signature.h transaction.c \
	verify.c rpmlock.c rpmlock.h misc.h relocation.c \
	rpmscript.h rpmscript.c legacy.c \
	rpmchroot.c rpmchroot.h \
	rpmplugins.c rpmplugins.h rpmplugin.h rpmug.c rpmug.h \
	rpmtriggers.h rpmtriggers.c

librpm_la_LDFLAGS = -version-info $(rpm_version_info)

librpm_la_LIBADD = \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_CAP_LIB@ \
	@WITH_ACL_LIB@ \
	@LIBINTL@

if WITH_IMAEVM
librpm_la_SOURCES += rpmsignfiles.c rpmsignfiles.h
endif

if WITH_LUA
AM_CPPFLAGS += @LUA_CFLAGS@
librpm_la_LIBADD += @LUA_LIBS@
librpm_la_SOURCES += rpmliblua.c rpmliblua.h
endif

if WITH_INTERNAL_DB
librpm_la_LIBADD += $(libdb_la)
else
librpm_la_LIBADD += @WITH_DB_LIB@
endif

if NDB
librpm_la_SOURCES += \
	backend/ndb/glue.c \
	backend/ndb/rpmpkg.c \
	backend/ndb/rpmpkg.h \
	backend/ndb/rpmidx.c \
	backend/ndb/rpmidx.h \
	backend/ndb/rpmxdb.c \
	backend/ndb/rpmxdb.h
endif

tagtbl.C: Makefile.am $(srcdir)/rpmtag.h gentagtbl.sh
	@AWK=${AWK} ${SHELL} $(srcdir)/gentagtbl.sh \
		    	     $(srcdir)/rpmtag.h > $@.new && \
	 mv -f $@.new $@
BUILT_SOURCES = tagtbl.C

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

rpmlibexec_PROGRAMS += rpmdb_dump
rpmdb_dump_SOURCES =
rpmdb_dump_LDADD = \
	$(top_builddir)/db3/db_dump.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpm.la

rpmlibexec_PROGRAMS += rpmdb_load
rpmdb_load_SOURCES =
rpmdb_load_LDADD = \
	$(top_builddir)/db3/db_load.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpm.la

rpmlibexec_PROGRAMS += rpmdb_recover
rpmdb_recover_SOURCES =
rpmdb_recover_LDADD = \
	$(top_builddir)/db3/db_recover.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpm.la

rpmlibexec_PROGRAMS += rpmdb_stat
rpmdb_stat_SOURCES =
rpmdb_stat_LDADD = \
	$(top_builddir)/db3/db_stat.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpm.la

rpmlibexec_PROGRAMS += rpmdb_upgrade
rpmdb_upgrade_SOURCES =
rpmdb_upgrade_LDADD = \
	$(top_builddir)/db3/db_upgrade.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpm.la

rpmlibexec_PROGRAMS += rpmdb_verify 
rpmdb_verify_SOURCES =
rpmdb_verify_LDADD = \
	$(top_builddir)/db3/db_verify.o \
	$(top_builddir)/db3/util_cache.o \
	$(top_builddir)/db3/util_sig.o \
	librpm.la
endif

CLEANFILES += $(BUILT_SOURCES)
