# Makefile for rpm scripts.

include $(top_srcdir)/rpm.am

bin_SCRIPTS =		gendiff

EXTRA_DIST = \
	gendiff \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-debuginfo.sh find-lang.sh 
	perldeps.pl perl.prov perl.req pythondeps.sh \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	tcl.req tgpg u_pkg.sh \
	vpkg-provides.sh vpkg-provides2.sh \
	macros.perl.in macros.python.in \
	macros.php.in find-requires.php find-provides.php \
	find-php-provides find-php-requires \
	mono-find-requires mono-find-provides \
	pkgconfigdeps.sh libtooldeps.sh

configdir = @RPMCONFIGDIR@
config_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files check-prereqs \
	check-buildroot check-rpaths check-rpaths-worker \
	find-lang.sh \
	perl.prov perl.req pythondeps.sh \
	mono-find-requires mono-find-provides \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	tcl.req tgpg 

## Build under same conditions as debugedit
if LIBELF
if LIBDWARF
config_SCRIPTS += find-debuginfo.sh
endif
endif
