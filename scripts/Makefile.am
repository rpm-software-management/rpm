# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = \
	brp-compress brp-strip brp-strip-comment-note \
	check-prereqs convertrpmrc.sh find-lang.sh find-prov.pl find-req.pl \
	freshen.sh get_magic.pl getpo.sh http.req magic.prov magic.req \
	perl.prov perl.req rpmdiff u_pkg.sh vpkg-provides.sh vpkg-provides2.sh

installprefix = $(DESTDIR)

all:

configdir = ${prefix}/lib/rpm
config_SCRIPTS = \
	brp-compress brp-strip brp-strip-comment-note \
	check-prereqs convertrpmrc.sh find-lang.sh find-prov.pl find-req.pl \
	freshen.sh get_magic.pl getpo.sh http.req magic.prov magic.req \
	perl.prov perl.req rpmdiff u_pkg.sh vpkg-provides.sh vpkg-provides2.sh
