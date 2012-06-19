# Makefile for rpm scripts.

include $(top_srcdir)/rpm.am

CLEANFILES =

EXTRA_DIST = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-python-hardlink \
	brp-strip-shared brp-strip-static-archive \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-debuginfo.sh find-lang.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh osgideps.pl \
	rpmdb_loadcvt rpm.daily rpm.log rpm.supp rpm2cpio.sh \
	tcl.req tgpg vpkg-provides.sh \
	find-requires.php find-provides.php \
	find-php-provides find-php-requires \
	mono-find-requires mono-find-provides \
	ocaml-find-requires.sh ocaml-find-provides.sh \
	pkgconfigdeps.sh libtooldeps.sh \
	fontconfig.prov desktop-file.prov script.req \
	macros.perl macros.php macros.python

rpmconfig_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-python-hardlink \
	brp-strip-shared brp-strip-static-archive \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh \
	perl.prov perl.req perldeps.pl pythondeps.sh osgideps.pl \
	mono-find-requires mono-find-provides \
	pkgconfigdeps.sh libtooldeps.sh \
	ocaml-find-requires.sh ocaml-find-provides.sh \
	fontconfig.prov desktop-file.prov script.req \
	rpmdb_loadcvt rpm2cpio.sh tcl.req tgpg 

rpmconfig_DATA = \
	rpm.daily rpm.log rpm.supp \
	macros.perl macros.php macros.python

