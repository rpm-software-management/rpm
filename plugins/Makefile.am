# Makefile for rpm library.

include $(top_srcdir)/rpm.am

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\""
AM_CPPFLAGS += -DLOCALSTATEDIR="\"$(localstatedir)\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"rpmpopt-${VERSION}\""

AM_LDFLAGS = -avoid-version -module -shared

pluginsdir = $(libdir)/rpm-plugins

plugins_LTLIBRARIES = exec.la

exec_la_SOURCES = plugin.h exec.c
exec_la_LIBADD = $(top_builddir)/lib/librpm.la $(top_builddir)/rpmio/librpmio.la

if SELINUX
sepolicy_la_SOURCES = plugin.h sepolicy.c
sepolicy_la_LIBADD = $(top_builddir)/lib/librpm.la $(top_builddir)/rpmio/librpmio.la @WITH_SELINUX_LIB@ @WITH_SEMANAGE_LIB@

plugins_LTLIBRARIES += sepolicy.la
endif
