INCLUDES = -I$(top_srcdir)/..

LIBS = -licuuc -licuio

AUTOMAKE_OPTIONS = gnu no-dependencies

pkgaltlibdir=$(prefix)/lib@LIBALT@
pkgaltlib_LTLIBRARIES = base.la

base_la_SOURCES = \
BeeCertificateFactory.cxx \
BeeCryptProvider.cxx \
BeeKeyFactory.cxx \
BeeKeyStore.cxx \
BeeSecureRandom.cxx \
DHKeyFactory.cxx \
DHKeyPairGenerator.cxx \
DHParameterGenerator.cxx \
DHParameters.cxx \
DHPrivateKeyImpl.cxx \
DHPublicKeyImpl.cxx \
DSAKeyFactory.cxx \
DSAKeyPairGenerator.cxx \
DSAParameterGenerator.cxx \
DSAParameters.cxx \
DSAPrivateKeyImpl.cxx \
DSAPublicKeyImpl.cxx \
HMACMD5.cxx \
HMACSHA1.cxx \
HMACSHA256.cxx \
KeyProtector.cxx \
MD5Digest.cxx \
MD5withRSASignature.cxx \
PKCS1RSASignature.cxx \
PKCS12KeyFactory.cxx \
RSAKeyFactory.cxx \
RSAKeyPairGenerator.cxx \
RSAPrivateCrtKeyImpl.cxx \
RSAPrivateKeyImpl.cxx \
RSAPublicKeyImpl.cxx \
SHA1Digest.cxx \
SHA1withDSASignature.cxx \
SHA1withRSASignature.cxx \
SHA256withRSASignature.cxx

base_la_LDFLAGS = -module
base_la_LIBADD = $(top_builddir)/c++/libbeecrypt_cxx.la

noinst_HEADERS = \
BeeCertificateFactory.h \
BeeCryptProvider.h \
BeeKeyFactory.h \
BeeKeyStore.h \
BeeSecureRandom.h \
DHKeyFactory.h \
DHKeyPairGenerator.h \
DHParameterGenerator.h \
DHParameters.h \
DHPrivateKeyImpl.h \
DHPublicKeyImpl.h \
DSAKeyFactory.h \
DSAKeyPairGenerator.h \
DSAParameterGenerator.h \
DSAParameters.h \
DSAPrivateKeyImpl.h \
DSAPublicKeyImpl.h \
HMACMD5.h \
HMACSHA1.h \
HMACSHA256.h \
KeyProtector.h \
MD5Digest.h \
MD5withRSASignature.h \
PKCS1RSASignature.h \
PKCS12KeyFactory.h \
RSAKeyFactory.h \
RSAKeyPairGenerator.h \
RSAPrivateCrtKeyImpl.h \
RSAPrivateKeyImpl.h \
RSAPublicKeyImpl.h \
SHA1Digest.h \
SHA1withDSASignature.h \
SHA1withRSASignature.h \
SHA256withRSASignature.h
