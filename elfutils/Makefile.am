## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits
MAINT_CHARSET = latin1

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 doc intl lib libltdl libelf libebl libdwarf libasm src \
	  libelf-po libebl-po libdwarf-po libasm-po po tests

EXTRA_DIST = config.rpath splint.rc

distcheck-hook:
	chmod -R u+w $(distdir)

# Tell version 3.79 and up of GNU make to not build goals in this
# directory in parallel.
.NOTPARALLEL:
