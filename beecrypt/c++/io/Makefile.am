INCLUDES = -I$(top_srcdir)/..

AUTOMAKE_OPTIONS = gnu no-dependencies

noinst_LTLIBRARIES = libcxxio.la
cxxiodir= $(pkgincludedir)/c++/io

libcxxio_la_SOURCES =\
ByteArrayInputStream.cxx \
ByteArrayOutputStream.cxx \
DataInputStream.cxx \
DataOutputStream.cxx \
EOFException.cxx \
FileInputStream.cxx \
FileOutputStream.cxx \
FilterInputStream.cxx \
FilterOutputStream.cxx \
InputStream.cxx \
IOException.cxx \
OutputStream.cxx \
PrintStream.cxx \
PushbackInputStream.cxx
libcxxio_la_LIBADD = -licuuc

cxxio_HEADERS =\
ByteArrayInputStream.h \
ByteArrayOutputStream.h \
DataInput.h \
DataOutput.h \
DataInputStream.h \
DataOutputStream.h \
EOFException.h \
FileInputStream.h \
FileOutputStream.h \
FilterInputStream.h \
FilterOutputStream.h \
InputStream.h \
IOException.h \
OutputStream.h \
PrintStream.h \
PushbackInputStream.h

TESTS = test

check_PROGRAMS = test

test_SOURCES = test.cxx
test_LDADD = libcxxio.la ../lang/libcxxlang.la $(top_builddir)/libbeecrypt.la -licuuc -licuio
