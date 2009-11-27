# Makefile for rpm scripts.

include $(top_srcdir)/rpm.am

CLEANFILES =

EXTRA_DIST = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-debuginfo.sh find-lang.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh osgideps.pl \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	tcl.req tgpg vpkg-provides.sh \
	find-requires.php find-provides.php \
	find-php-provides find-php-requires \
	mono-find-requires mono-find-provides \
	ocaml-find-requires.sh ocaml-find-provides.sh \
	pkgconfigdeps.sh libtooldeps.sh \
	fontconfig.prov desktop-file.prov

rpmconfig_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh \
	perl.prov perl.req perldeps.pl pythondeps.sh osgideps.pl \
	mono-find-requires mono-find-provides \
	pkgconfigdeps.sh libtooldeps.sh \
	ocaml-find-requires.sh ocaml-find-provides.sh \
	fontconfig.prov desktop-file.prov \
	rpmdb_loadcvt rpmdiff rpm2cpio.sh tcl.req tgpg 

rpmconfig_DATA = \
	rpmdiff.cgi rpm.daily rpm.log rpm.xinetd

rpmconfig_DATA += macros.perl
macros.perl: $(srcdir)/macros.perl.in
	@cat \
	  < $(srcdir)/macros.perl.in > macros.perl
CLEANFILES += macros.perl
EXTRA_DIST += macros.perl.in

rpmconfig_DATA += macros.php
macros.php: $(srcdir)/macros.php.in
	@cat \
	  < $(srcdir)/macros.php.in > macros.php
CLEANFILES += macros.php
EXTRA_DIST += macros.php.in

rpmconfig_DATA += macros.python
macros.python: $(srcdir)/macros.python.in
	@cat \
	  < $(srcdir)/macros.python.in > macros.python
CLEANFILES += macros.python
EXTRA_DIST += macros.python.in
