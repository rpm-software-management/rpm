# Makefile for rpm scripts.

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

CLEANFILES =

EXTRA_DIST = \
	brp-compress \
	brp-strip brp-strip-comment-note \
	brp-strip-static-archive brp-elfperms \
	brp-remove-la-files \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh \
	perl.prov perl.req \
	rpmdb_dump rpmdb_load \
	rpm.daily rpm.log rpm.supp rpm2cpio.sh \
	tgpg vpkg-provides.sh \
	find-requires find-provides \
	ocamldeps.sh \
	pkgconfigdeps.sh \
	fontconfig.prov script.req

rpmconfig_SCRIPTS = \
	brp-compress \
	brp-strip brp-strip-comment-note \
	brp-strip-static-archive brp-elfperms \
	brp-remove-la-files \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh find-requires find-provides \
	perl.prov perl.req \
	pkgconfigdeps.sh \
	ocamldeps.sh \
	fontconfig.prov script.req \
	rpmdb_dump rpmdb_load \
	rpm2cpio.sh tgpg

rpmconfig_DATA = \
	rpm.daily rpm.log rpm.supp

