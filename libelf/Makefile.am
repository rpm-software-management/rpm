# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

SUBDIRS = lib @POSUB@

EXTRA_DIST = \
	ChangeLog COPYING.LIB INSTALL README VERSION \
	libelf.spec po/*.in po/*.po po/@PACKAGE@.pot

.PHONY: sources
sources:
	make -C lib sources

.PHONY: lclint
lclint:
	make -C lib lclint
