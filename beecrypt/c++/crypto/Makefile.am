INCLUDES = -I$(top_srcdir)/..

AUTOMAKE_OPTIONS = gnu no-dependencies

SUBDIRS = interfaces spec

noinst_LTLIBRARIES = libcxxcrypto.la

cxxcryptodir=$(pkgincludedir)/c++/crypto

libcxxcrypto_la_SOURCES = \
BadPaddingException.cxx \
Mac.cxx \
MacInputStream.cxx \
MacOutputStream.cxx \
SecretKeyFactory.cxx
libcxxcrypto_la_LIBADD = spec/libcxxcryptospec.la

cxxcrypto_HEADERS = \
BadPaddingException.h \
Mac.h \
MacInputStream.h \
MacOutputStream.h \
MacSpi.h \
SecretKey.h \
SecretKeyFactory.h \
SecretKeyFactorySpi.h
