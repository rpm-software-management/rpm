# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = db3.c db2.c db1.c falloc.c

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmdb.h
noinst_HEADERS = falloc.h

mylibpaths = \
	-L$(top_builddir)/lib/.libs \
	-L$(top_builddir)/rpmio/.libs \
	-L$(top_builddir)/popt/.libs

mylibs = -lrpm -lrpmio -lpopt @LIBS@ @INTLLIBS@ @LIBMISC@
LIBS =

DB3LOBJS= \
	mut_tas.lo    bt_compare.lo bt_conv.lo \
	bt_curadj.lo bt_cursor.lo bt_delete.lo bt_method.lo bt_open.lo \
	bt_put.lo bt_rec.lo bt_reclaim.lo bt_recno.lo bt_rsearch.lo \
	bt_search.lo bt_split.lo bt_stat.lo bt_upgrade.lo bt_verify.lo \
	btree_auto.lo crdel_auto.lo crdel_rec.lo db.lo db_am.lo db_auto.lo \
	db_byteorder.lo db_cam.lo db_conv.lo db_dispatch.lo db_dup.lo \
	db_err.lo db_getlong.lo db_iface.lo db_join.lo db_log2.lo \
	db_meta.lo db_method.lo db_overflow.lo db_pr.lo db_rec.lo \
	db_reclaim.lo db_ret.lo db_salloc.lo db_shash.lo db_upg.lo \
	db_upg_opd.lo db_vrfy.lo db_vrfyutil.lo dbm.lo env_method.lo \
	env_open.lo env_recover.lo env_region.lo hash.lo hash_auto.lo \
	hash_conv.lo hash_dup.lo hash_func.lo hash_meta.lo hash_method.lo \
	hash_page.lo hash_rec.lo hash_reclaim.lo hash_stat.lo hash_upgrade.lo \
	hash_verify.lo hsearch.lo lock.lo lock_conflict.lo \
	lock_deadlock.lo lock_method.lo lock_region.lo lock_stat.lo \
	lock_util.lo log.lo log_archive.lo log_auto.lo log_compare.lo \
	log_findckp.lo log_get.lo log_method.lo log_put.lo log_rec.lo \
	log_register.lo mp_alloc.lo mp_bh.lo mp_fget.lo mp_fopen.lo \
	mp_fput.lo mp_fset.lo mp_method.lo mp_region.lo mp_register.lo \
	mp_stat.lo mp_sync.lo mp_trickle.lo mutex.lo os_abs.lo \
	os_alloc.lo os_dir.lo os_errno.lo os_fid.lo os_finit.lo \
	os_fsync.lo os_handle.lo os_map.lo os_method.lo os_oflags.lo \
	os_open.lo os_region.lo os_rename.lo os_root.lo os_rpath.lo \
	os_rw.lo os_seek.lo os_sleep.lo os_spin.lo os_stat.lo \
	os_tmpdir.lo os_unlink.lo qam.lo qam_auto.lo qam_conv.lo qam_files.lo \
	qam_method.lo qam_open.lo qam_rec.lo qam_stat.lo qam_upgrade.lo \
	qam_verify.lo txn.lo txn_auto.lo txn_rec.lo txn_region.lo xa.lo \
	xa_db.lo xa_map.lo

lib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = $(DBLIBSRCS) dbconfig.c rpmdb.c
librpmdb_la_LDFLAGS = @libdb3@ @libdb2@ @libdb1@
librpmdb_la_LIBADD = $(DBLIBOBJS) $(DB3LOBJS)
librpmdb_la_DEPENDENCIES = $(DBLIBOBJS) symlinkdb3

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpmio|" < librpmdb.la > .librpmdb.la && \
	mv .librpmdb.la librpmdb.la

falloc.lo: falloc.c $(top_srcdir)/system.h $(top_srcdir)/rpmio/rpmio.h falloc.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.PHONY:	symlinkdb3
symlinkdb3:
	for lo in $(DB3LOBJS); do \
	  [ -e $$lo ] || $(LN_S) $(top_builddir)/$(WITH_DB_SUBDIR)/$$lo $$lo ; \
	done

distclean-local:
	rm -f $(DB3LOBJS)

.PHONY:	sources
sources:
	@echo $(librpmdb_la_SOURCES:%=rpmdb/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmdb_la_SOURCES)
