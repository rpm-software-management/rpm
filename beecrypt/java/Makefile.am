#
# Copyright (c) 2003 Bob Deblier
#
# Author: Bob Deblier <bob.deblier@pandora.be>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

LIBBEECRYPT_LT_CURRENT = 6
LIBBEECRYPT_LT_AGE = 0
LIBBEECRYPT_LT_REVISION = 0

AUTOMAKE_OPTIONS = gnu no-dependencies

BEECRYPT_JAVA_OBJECTS = javaglue.lo

lib_LTLIBRARIES = libbeecrypt_java.la

noinst_HEADERS = javaglue.h

libbeecrypt_java_la_SOURCES = javaglue.c

libbeecrypt_java_la_DEPENDENCIES = $(BEECRYPT_JAVA_OBJECTS)

libbeecrypt_java_la_LDFLAGS = -no-undefined -version-info $(LIBBEECRYPT_LT_CURRENT):$(LIBBEECRYPT_LT_REVISION):$(LIBBEECRYPT_LT_AGE)
