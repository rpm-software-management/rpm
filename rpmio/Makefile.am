# Makefile for rpm library.

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

EXTRA_DIST = modemuncher.c

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/include/
AM_CPPFLAGS += @WITH_OPENSSL_INCLUDE@
AM_CPPFLAGS += @WITH_LIBGCRYPT_INCLUDE@
AM_CPPFLAGS += @WITH_RPM_SEQUOIA_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += $(ZSTD_CFLAGS)
AM_CPPFLAGS += @LUA_CFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""
AM_CPPFLAGS += -DLOCALSTATEDIR="\"$(localstatedir)\""

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c base64.c digest.c expression.c macro.c \
	rpmhook.c rpmio.c rpmlog.c rpmmalloc.c rgetopt.c \
	rpmpgp.c rpmpgpval.h rpmsq.c rpmsw.c url.c \
	rpmio_internal.h rpmhook.h rpmvercmp.c rpmver.c \
	rpmstring.c rpmfileutil.c rpmglob.c \
	rpmkeyring.c rpmstrpool.c rpmmacro_internal.h \
	rpmlua.c rpmlua.h lposix.c lposix.h

if WITH_OPENSSL
librpmio_la_SOURCES += digest_openssl.c rpmpgp_internal.c rpmpgp_internal.h
else
if WITH_LIBGCRYPT
librpmio_la_SOURCES += digest_libgcrypt.c rpmpgp_internal.c rpmpgp_internal.h
else
if WITH_RPM_SEQUOIA
librpmio_la_SOURCES += rpmpgp_sequoia.c
endif
endif
endif

librpmio_la_LDFLAGS = -version-info $(rpm_version_info)
librpmio_la_LIBADD = \
	../misc/libmisc.la \
	@WITH_OPENSSL_LIB@ \
	@WITH_LIBGCRYPT_LIB@ \
	@WITH_RPM_SEQUOIA_LIB@ \
	@WITH_BZ2_LIB@ \
	@WITH_ZLIB_LIB@ \
	@WITH_POPT_LIB@ \
	@WITH_LZMA_LIB@ \
	@LUA_LIBS@ \
	$(ZSTD_LIBS) \
	-lpthread

check_PROGRAMS =

