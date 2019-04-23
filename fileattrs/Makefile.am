# Makefile for rpm file attributes

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

fattrsdir = $(rpmconfigdir)/fileattrs

fattrs_DATA = \
	debuginfo.attr desktop.attr elf.attr font.attr libtool.attr metainfo.attr \
	perl.attr perllib.attr pkgconfig.attr python.attr pythondist.attr ocaml.attr \
	script.attr

EXTRA_DIST = $(fattrs_DATA)
