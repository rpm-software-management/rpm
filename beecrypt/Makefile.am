#
# Makefile.am contains the top-level automake definitions
#
# Copyright (c) 2001 Virtual Unlimited B.V.
#
# Author: Bob Deblier <bob@virtualunlimited.com>
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

#
# 1. No interfaces changes (good): Increment REVISION
#
# 2. Interfaces added, none removed (good): Increment CURRENT, increment AGE and REVISION to 0.
#
# 3. Interfaces removed (bad): Increment CURRENT, set AGE and REVISION to 0.
#

LIBBEECRYPT_LT_CURRENT = 4
LIBBEECRYPT_LT_AGE = 2
LIBBEECRYPT_LT_REVISION = 0

AUTOMAKE_OPTIONS = gnu check-news no-dependencies

SUBDIRS = docs gas masm mwerks tests

SUFFIXES = .S
.S.lo:
	$(LTCOMPILE) -c -o $@ `test -f $< || echo '$(srcdir)/'`$<

BEECRYPT_OBJECTS = base64.lo beecrypt.lo blockmode.lo blockpad.lo blowfish.lo blowfishopt.lo dhaes.lo dldp.lo dlkp.lo dlpk.lo dlsvdp-dh.lo dsa.lo elgamal.lo endianness.lo entropy.lo fips180.lo fips180opt.lo fips186.lo hmac.lo hmacmd5.lo hmacsha1.lo hmacsha256.lo md5.lo memchunk.lo mp32.lo mp32barrett.lo mp32number.lo mp32opt.lo mp32prime.lo mtprng.lo rsa.lo rsakp.lo rsapk.lo sha256.lo timestamp.lo

BEECRYPT_JAVA_OBJECTS = javaglue.lo

lib_LTLIBRARIES = libbeecrypt.la

libbeecrypt_la_SOURCES = base64.c beecrypt.c blockmode.c blockpad.c blowfish.c dhaes.c dldp.c dlkp.c dlpk.c dlsvdp-dh.c dsa.c elgamal.c endianness.c entropy.c fips180.c fips186.c hmac.c hmacmd5.c hmacsha1.c hmacsha256.c javaglue.c md5.c memchunk.c mp32.c mp32barrett.c mp32number.c mp32prime.c mtprng.c rsa.c rsakp.c rsapk.c sha256.c timestamp.c

libbeecrypt_la_DEPENDENCIES = $(BEECRYPT_OBJECTS) $(BEECRYPT_JAVA_OBJECTS)

libbeecrypt_la_LIBADD = blowfishopt.lo fips180opt.lo mp32opt.lo

libbeecrypt_la_LDFLAGS = -version-info $(LIBBEECRYPT_LT_CURRENT):$(LIBBEECRYPT_LT_REVISION):$(LIBBEECRYPT_LT_AGE)

pkginclude_HEADERS = base64.h beecrypt.h blockmode.h blockpad.h blowfish.h blowfishopt.h dhaes.h dldp.h dlkp.h dlpk.h dlsvdp-dh.h dsa.h elgamal.h endianness.h entropy.h fips180.h fips180opt.h fips186.h hmac.h hmacmd5.h hmacsha1.h hmacsha256.h md5.h memchunk.h mp32.h mp32barrett.h mp32number.h mp32opt.h mp32prime.h mtprng.h rsa.h rsakp.h rsapk.h sha256.h timestamp.h

EXTRA_DIST = BENCHMARKS BUGS CONTRIBUTORS Doxyfile.in Doxyheader README.DLL README.WIN32 beecrypt.def beecrypt.mcp beecrypt.rc beecrypt.spec config.h config.gas.h config.win.h javaglue.h

.PHONY: beetest
beetest: all
	make -C tests beetest
	./tests/beetest

.PHONY: sources
sources:
	@echo $(libbeecrypt_la_SOURCES:%=beecrypt/%) | sed -e 's,beecrypt/javaglue.c,,'

.PHONY: lclint
lclint:
	lclint $(DEFS) $(INCLUDES) ${libbeecrypt_la_SOURCES} tests/beetest.c

.PHONY: listobjs
listobjs:
	@echo $(BEECRYPT_OBJECTS) > $@

DOXYGEN = /usr/bin/doxygen
.PHONY: doxygen
doxygen apidocs: Doxyfile Doxyheader
	rm -rf $@
	mkdir -p $@
	- [ -x ${DOXYGEN} ] && ${DOXYGEN}
