INCLUDES = -I$(top_srcdir)/..

AUTOMAKE_OPTIONS = gnu no-dependencies

noinst_LTLIBRARIES = libcxxsecuritycert.la

cxxsecuritycertdir=$(pkgincludedir)/c++/security/cert

libcxxsecuritycert_la_SOURCES =\
Certificate.cxx \
CertificateException.cxx \
CertificateExpiredException.cxx \
CertificateFactory.cxx \
CertificateNotYetValidException.cxx

cxxsecuritycert_HEADERS =\
Certificate.h \
CertificateException.h \
CertificateExpiredException.h \
CertificateFactory.h \
CertificateFactorySpi.h \
CertificateNotYetValidException.h
