# Makefile for rpm scripts.

include $(top_srcdir)/rpm.am

CLEANFILES =

EXTRA_DIST = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-debuginfo.sh find-lang.sh \
	perldeps.pl perl.prov perl.req pythondeps.sh osgideps.pl \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	tcl.req tgpg u_pkg.sh \
	vpkg-provides.sh vpkg-provides2.sh \
	find-requires.php find-provides.php \
	find-php-provides find-php-requires \
	mono-find-requires mono-find-provides \
	pkgconfigdeps.sh libtooldeps.sh

rpmconfig_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh \
	perl.prov perl.req pythondeps.sh osgideps.pl \
	mono-find-requires mono-find-provides \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	tcl.req tgpg 

rpmconfig_SCRIPTS += macros.perl
macros.perl: $(srcdir)/macros.perl.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir),g" \
	  < $(srcdir)/macros.perl.in > macros.perl
CLEANFILES += macros.perl
EXTRA_DIST += macros.perl.in

rpmconfig_SCRIPTS += macros.php
macros.php: $(srcdir)/macros.php.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir),g" \
	  < $(srcdir)/macros.php.in > macros.php
CLEANFILES += macros.php
EXTRA_DIST += macros.php.in

rpmconfig_SCRIPTS += macros.python
macros.python: $(srcdir)/macros.python.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir),g" \
	  < $(srcdir)/macros.python.in > macros.python
CLEANFILES += macros.python
EXTRA_DIST += macros.python.in

rpmconfig_SCRIPTS += pkgconfigdeps.sh
rpmconfig_SCRIPTS += libtooldeps.sh
