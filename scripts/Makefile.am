# Makefile for rpm scripts.

include $(top_srcdir)/rpm.am

CLEANFILES =

EXTRA_DIST = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-python-hardlink \
	brp-strip-shared brp-strip-static-archive \
	brp-clean-files brp-remove-info-dir brp-relink-symlinks \
	brp-remove-libtool-files brp-fix-eol brp-remove-rpath \
	brp-fix-file-permissions brp-fix-desktop-files brp-fix-pkgconfig \
	brp-lib-symlinks brp-check-elf-files-for-unused-libraries \
	brp-check-elf-files-for-undefined-symbols \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-debuginfo.sh find-lang.sh \
	perl.prov perl.req pythondeps.sh pythoneggs.py \
	rpmdb_loadcvt rpm.daily rpm.log rpm.supp rpm2cpio.sh \
	tgpg vpkg-provides.sh \
	find-requires find-provides \
	find-requires.php find-provides.php \
	mono-find-requires mono-find-provides \
	ocaml-find-requires.sh ocaml-find-provides.sh \
	pkgconfigdeps.sh libtooldeps.sh appdata.prov \
	fontconfig.prov desktop-file.prov script.req \
	macros.perl macros.php macros.python

rpmconfig_SCRIPTS = \
	appdata.prov \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note brp-python-hardlink \
	brp-strip-shared brp-strip-static-archive \
	brp-clean-files brp-remove-info-dir brp-relink-symlinks \
	brp-remove-libtool-files brp-fix-eol brp-remove-rpath \
	brp-fix-file-permissions brp-fix-desktop-files brp-fix-pkgconfig \
	brp-lib-symlinks brp-check-elf-files-for-unused-libraries \
	brp-check-elf-files-for-undefined-symbols \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh find-requires find-provides \
	perl.prov perl.req pythondeps.sh pythoneggs.py \
	mono-find-requires mono-find-provides \
	pkgconfigdeps.sh libtooldeps.sh \
	ocaml-find-requires.sh ocaml-find-provides.sh \
	fontconfig.prov desktop-file.prov script.req \
	rpmdb_loadcvt rpm2cpio.sh tgpg 

rpmconfig_DATA = \
	rpm.daily rpm.log rpm.supp \
	macros.perl macros.php macros.python

