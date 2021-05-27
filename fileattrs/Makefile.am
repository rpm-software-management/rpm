# Makefile for rpm file attributes

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

fattrsdir = $(rpmconfigdir)/fileattrs

fattrs_DATA = \
	debuginfo.attr desktop.attr elf.attr font.attr metainfo.attr \
	perl.attr perllib.attr pkgconfig.attr ocaml.attr \
	script.attr

EXTRA_DIST = $(fattrs_DATA)
