# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

bin_SCRIPTS =		gendiff

EXTRA_DIST = \
	gendiff \
	brp-compress brp-python-bytecompile brp-java-gcjcompile brp-redhat \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files check-prereqs convertrpmrc.sh cross-build \
	find-debuginfo.sh find-lang.sh find-prov.pl find-req.pl \
	cpanflute cpanflute2 Specfile.pm find-provides.perl \
	find-requires.perl freshen.sh get_magic.pl getpo.sh http.req \
	libtool.*.sh pkgconfig.*.sh \
	magic.prov magic.req perldeps.pl perl.prov perl.req pythondeps.sh \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	sql.prov sql.req tcl.req tgpg trpm u_pkg.sh \
	vpkg-provides.sh vpkg-provides2.sh \
	macros.perl* macros.python* \
	macros.php* find-*.php find-php-*

installprefix = $(DESTDIR)

all:

configdir = ${prefix}/lib/rpm
config_SCRIPTS = \
	brp-compress brp-python-bytecompile brp-java-gcjcompile brp-redhat \
	brp-strip brp-strip-comment-note \
	brp-strip-shared brp-strip-static-archive brp-sparc64-linux \
	check-files check-prereqs convertrpmrc.sh cross-build \
	find-debuginfo.sh find-lang.sh find-prov.pl find-req.pl \
	cpanflute cpanflute2 Specfile.pm find-provides.perl \
	find-requires.perl freshen.sh get_magic.pl getpo.sh http.req \
	magic.prov magic.req perldeps.pl perl.prov perl.req pythondeps.sh \
	rpmdb_loadcvt rpmdiff rpmdiff.cgi \
	rpm.daily rpm.log rpm.xinetd rpm2cpio.sh \
	sql.prov sql.req tcl.req tgpg trpm u_pkg.sh \
	vpkg-provides.sh vpkg-provides2.sh
