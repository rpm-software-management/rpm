INCLUDES = -I$(top_srcdir)/..

LIBS = -licuuc -licuio

AUTOMAKE_OPTIONS = gnu no-dependencies

noinst_LTLIBRARIES = libcxxbeeyond.la

libcxxbeeyond_la_SOURCES = \
AnyEncodedKeySpec.cxx \
BeeCertificate.cxx \
BeeEncodedKeySpec.cxx \
BeeInputStream.cxx \
BeeOutputStream.cxx \
PKCS12PBEKey.cxx

noinst_HEADERS = \
AnyEncodedKeySpec.h \
BeeCertificate.h \
BeeEncodedKeySpec.h \
BeeInputStream.h \
BeeOutputStream.h \
PKCS12PBEKey.h

TESTS = testcert

check_PROGRAMS = testcert

testcert_SOURCES = testcert.cxx
testcert_LDADD = ../libbeecrypt_cxx.la
