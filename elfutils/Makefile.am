## Process this file with automake to create Makefile.in

ACLOCAL_AMFLAGS = -I m4

#SUBDIRS = m4 doc lib libltdl libelf libebl libdwarf libcpu libasm src \
#	  libelf-po libebl-po libdwarf-po libasm-po po tests

SUBDIRS = m4 doc lib libltdl libelf libebl libdwarf libelf-po libebl-po libdwarf-po po

EXTRA_DIST = config.rpath splint.rc elfutils.spec GPG-KEY NOTES

distcheck-hook:
	chmod -R u+w $(distdir)

# Tell version 3.79 and up of GNU make to not build goals in this
# directory in parallel.
.NOTPARALLEL:
