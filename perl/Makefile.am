# Makefile for rpm scripts.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = \
	db.xs header.xs junk.xs rpm.xs transaction.xs

installprefix = $(DESTDIR)

configdir = ${prefix}/lib/rpm
config_SCRIPTS = \
	db.xs header.xs junk.xs rpm.xs transaction.xs

all install clean: PMakefile
	make -f $< $@
	
PMakefile: Makefile.PL
	perl Makefile.PL
