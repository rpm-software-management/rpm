# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/misc \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST =	fstrcmp.c message.c str-list.c rpmchecksig.c
mylibs=	$(top_builddir)/build/.libs/librpmbuild.a \
	$(top_builddir)/lib/.libs/librpm.a \
	$(top_builddir)/popt/.libs/libpopt.a

LDADD = $(top_builddir)/lib/.libs/librpm.a

noinst_PROGRAMS = \
	dump dumpdb javadeps rpmarchive rpmheader rpminject rpmlead rpmsignature

rpminject_LDADD =	$(mylibs) @LIBMISC@

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = rpmgettext rpmputtext

rpmgettext_SOURCES =	rpmgettext.c
rpmgettext_LDADD =	$(mylibs) @LIBMISC@

rpmputtext_SOURCES =	rpmgettext.c
rpmputtext_LDADD =	$(mylibs) @LIBMISC@

## Dumpdb requires fprint.o (in libmisc) and this requires realpath
## (from libmisc).
dumpdb_LDADD = $(mylibs) @LIBMISC@

$(PROGRAMS): $(mylibs)
