#
# Makefile.am contains the top-level automake definitions
#
# Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

#
# 1. No interfaces changes (good): Increment REVISION
#
# 2. Interfaces added, none removed (good): Increment CURRENT, increment AGE and REVISION to 0.
#
# 3. Interfaces removed (bad): Increment CURRENT, set AGE and REVISION to 0.
#

LIBBEECRYPT_LT_CURRENT = 8
LIBBEECRYPT_LT_AGE = 2
LIBBEECRYPT_LT_REVISION = 0

AUTOMAKE_OPTIONS = gnu check-news no-dependencies

SUBDIRS = . $(MAYBE_SUB) tests docs gas masm

SUFFIXES = .s

INCLUDES = -I$(top_srcdir)/..

.s.lo:
	$(LTCOMPILE) -c -o $@ `test -f $< || echo '$(srcdir)/'`$<

BEECRYPT_OBJECTS = aes.lo aesopt.lo base64.lo beecrypt.lo blockmode.lo blockpad.lo blowfish.lo blowfishopt.lo dhaes.lo dldp.lo dlkp.lo dlpk.lo dlsvdp-dh.lo dsa.lo elgamal.lo endianness.lo entropy.lo fips186.lo hmac.lo hmacmd5.lo hmacsha1.lo hmacsha256.lo md5.lo memchunk.lo mp.lo mpopt.lo mpbarrett.lo mpnumber.lo mpprime.lo mtprng.lo pkcs1.lo pkcs12.lo rsa.lo rsakp.lo rsapk.lo sha1.lo sha1opt.lo sha256.lo timestamp.lo

libaltdir=$(prefix)/lib@LIBALT@

libalt_LTLIBRARIES = libbeecrypt.la

libbeecrypt_la_SOURCES = aes.c base64.c beecrypt.c blockmode.c blockpad.c blowfish.c dhaes.c dldp.c dlkp.c dlpk.c dlsvdp-dh.c dsa.c elgamal.c endianness.c entropy.c fips186.c hmac.c hmacmd5.c hmacsha1.c hmacsha256.c md5.c memchunk.c mp.c mpbarrett.c mpnumber.c mpprime.c mtprng.c pkcs1.c pkcs12.c rsa.c rsakp.c rsapk.c sha1.c sha256.c timestamp.c cppglue.cxx
libbeecrypt_la_DEPENDENCIES = $(BEECRYPT_OBJECTS)
libbeecrypt_la_LIBADD = aesopt.lo blowfishopt.lo mpopt.lo sha1opt.lo
libbeecrypt_la_LDFLAGS = -no-undefined -version-info $(LIBBEECRYPT_LT_CURRENT):$(LIBBEECRYPT_LT_REVISION):$(LIBBEECRYPT_LT_AGE)

pkginclude_HEADERS = aes.h aesopt.h api.h base64.h beecrypt.h blockmode.h blockpad.h blowfish.h blowfishopt.h dhaes.h dldp.h dlkp.h dlpk.h dlsvdp-dh.h dsa.h elgamal.h endianness.h entropy.h fips186.h gnu.h hmac.h hmacmd5.h hmacsha1.h hmacsha256.h md5.h memchunk.h mp.h mpbarrett.h mpnumber.h mpopt.h mpprime.h mtprng.h pkcs1.h pkcs12.h rsa.h rsakp.h rsapk.h sha1.h sha1opt.h sha256.h timestamp.h win.h

noinst_HEADERS = aes_be.h aes_le.h

EXTRA_DIST = BENCHMARKS BUGS CONTRIBUTORS README.DLL README.WIN32 Doxyheader beecrypt.rc autogen.sh Makefile.mak

DISTCLEANFILES = mpopt.s aesopt.s blowfishopt.s sha1opt.s

bench:
	(cd tests && $(MAKE) $(AM_MAKEFLAGS) bench)
