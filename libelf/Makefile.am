# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

SUBDIRS = lib @POSUB@

EXTRA_DIST = \
	ChangeLog COPYING.LIB INSTALL README VERSION \
	libelf.spec po/*.in po/*.po po/@PACKAGE@.pot

BUILT_SOURCES = elf_repl.h gelf.h libelf.h nlist.h sys_elf.h

libelf.lcd: Makefile.am
	make -C lib libelf.lcd
	cp lib/libelf.lcd .

elf_repl.h: ./lib/elf_repl.h
	ln -sf $< $@

gelf.h: ./lib/gelf.h
	ln -sf $< $@

libelf.h: ./lib/libelf.h
	ln -sf $< $@

nlist.h: ./lib/nlist.h
	ln -sf $< $@

sys_elf.h: ./lib/sys_elf.h
	ln -sf $< $@

.PHONY: sources
sources:
	make -C lib sources

.PHONY: lclint
lclint:
	make -C lib lclint
