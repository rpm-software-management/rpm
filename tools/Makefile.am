# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt \
	@INCPATH@ \
	-I/usr/include \
	-I$(top_srcdir)/misc

EXTRA_DIST =	fstrcmp.c message.c str-list.c rpmchecksig.c
mylibs=	$(top_builddir)/build/.libs/librpmbuild.a \
	$(top_builddir)/lib/.libs/librpm.a \
	$(top_builddir)/popt/.libs/libpopt.a \
	@LIBMISC@

LDADD = $(top_builddir)/lib/.libs/librpm.a @LIBMISC@

noinst_PROGRAMS = \
	dump dumpdb javadeps rpmarchive rpmheader rpminject rpmlead rpmsignature

rpminject_LDADD =	$(mylibs)

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = rpmgettext rpmputtext

rpmgettext_SOURCES =	rpmgettext.c
rpmgettext_LDADD =	$(mylibs)

rpmputtext_SOURCES =	rpmgettext.c
rpmputtext_LDADD =	$(mylibs)

## Dumpdb requires fprint.o (in libmisc) and this requires realpath
## (from libmisc).
dumpdb_LDADD = $(mylibs)

$(PROGRAMS): $(mylibs)
