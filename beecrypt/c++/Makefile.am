LIBBEECRYPT_CXX_LT_CURRENT = 8
LIBBEECRYPT_CXX_LT_AGE = 2
LIBBEECRYPT_CXX_LT_REVISION = 0

INCLUDES = -I$(top_srcdir)/..

AUTOMAKE_OPTIONS = gnu no-dependencies

SUBDIRS = lang io util security crypto beeyond . provider

libaltdir=$(prefix)/lib@LIBALT@
cxxdir=$(pkgincludedir)/c++

libalt_LTLIBRARIES = libbeecrypt_cxx.la

libbeecrypt_cxx_la_SOURCES = \
adapter.cxx \
bstream.cxx \
resource.cxx

libbeecrypt_cxx_la_LIBADD = ../libbeecrypt.la beeyond/libcxxbeeyond.la crypto/libcxxcrypto.la io/libcxxio.la lang/libcxxlang.la util/libcxxutil.la security/libcxxsecurity.la -licuuc -licuio
libbeecrypt_cxx_la_LDFLAGS = -no-undefined -version-info $(LIBBEECRYPT_CXX_LT_CURRENT):$(LIBBEECRYPT_CXX_LT_REVISION):$(LIBBEECRYPT_CXX_LT_AGE)

cxx_HEADERS = \
array.h \
bstream.h \
mutex.h

noinst_HEADERS = \
adapter.h \
resource.h

TESTS = testdsa testrsa testks

check_PROGRAMS = testdsa testrsa testks

testdsa_SOURCES = testdsa.cxx
testdsa_LDADD = libbeecrypt_cxx.la

testrsa_SOURCES = testrsa.cxx
testrsa_LDADD = libbeecrypt_cxx.la

testks_SOURCES = testks.cxx
testks_LDADD = libbeecrypt_cxx.la
