INCLUDES = -I$(top_srcdir)/..

AUTOMAKE_OPTIONS = gnu no-dependencies

SUBDIRS = cert interfaces spec

noinst_LTLIBRARIES = libcxxsecurity.la

cxxsecuritydir=$(pkgincludedir)/c++/security

libcxxsecurity_la_SOURCES =\
AlgorithmParameterGenerator.cxx \
AlgorithmParameters.cxx \
DigestInputStream.cxx \
DigestOutputStream.cxx \
GeneralSecurityException.cxx \
InvalidAlgorithmParameterException.cxx \
InvalidKeyException.cxx \
InvalidParameterException.cxx \
KeyException.cxx \
KeyFactory.cxx \
KeyPair.cxx \
KeyPairGenerator.cxx \
KeyStore.cxx \
KeyStoreException.cxx \
MessageDigest.cxx \
NoSuchAlgorithmException.cxx \
NoSuchProviderException.cxx \
Provider.cxx \
SecureRandom.cxx \
Security.cxx \
ShortBufferException.cxx \
Signature.cxx \
SignatureException.cxx \
UnrecoverableKeyException.cxx
libcxxsecurity_la_LIBADD = cert/libcxxsecuritycert.la spec/libcxxsecurityspec.la

cxxsecurity_HEADERS =\
AlgorithmParameterGenerator.h \
AlgorithmParameterGeneratorSpi.h \
AlgorithmParameters.h \
AlgorithmParametersSpi.h \
DigestInputStream.h \
DigestOutputStream.h \
GeneralSecurityException.h \
InvalidAlgorithmParameterException.h \
InvalidKeyException.h \
InvalidParameterException.h \
KeyFactory.h \
KeyFactorySpi.h \
Key.h \
KeyException.h \
KeyPairGenerator.h \
KeyPairGeneratorSpi.h \
KeyPair.h \
KeyStoreException.h \
KeyStore.h \
KeyStoreSpi.h \
MessageDigest.h \
MessageDigestSpi.h \
NoSuchAlgorithmException.h \
NoSuchProviderException.h \
PrivateKey.h \
Provider.h \
PublicKey.h \
SecureRandom.h \
SecureRandomSpi.h \
Security.h \
ShortBufferException.h \
Signature.h \
SignatureException.h \
SignatureSpi.h \
UnrecoverableKeyException.h
