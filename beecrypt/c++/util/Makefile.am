INCLUDES = -I$(top_srcdir)/..

AUTOMAKE_OPTIONS = gnu no-dependencies

noinst_LTLIBRARIES = libcxxutil.la
cxxutildir= $(pkgincludedir)/c++/util

libcxxutil_la_SOURCES =\
Date.cxx \
NoSuchElementException.cxx \
Properties.cxx

cxxutil_HEADERS =\
Date.h \
Enumeration.h \
NoSuchElementException.h \
Properties.h

TESTS = testdate testprop

check_PROGRAMS = testdate testprop

testdate_SOURCES = testdate.cxx
testdate_LDADD = ../libbeecrypt_cxx.la

testprop_SOURCES = testprop.cxx
testprop_LDADD = ../libbeecrypt_cxx.la
