# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

SUBDIRS = lib @POSUB@

EXTRA_DIST = \
	aclocal.m4 ChangeLog config.guess config.h.in \
	config.sub configure configure.in COPYING.LIB INSTALL install-sh \
	Makefile.in mkinstalldirs README stamp-h.in VERSION \
	libelf.spec po/*.in po/*.po po/@PACKAGE@.pot

.PHONY: sources
sources:
	make -C lib sources

.PHONY: lclint
lclint:
	make -C lib lclint
