#
# Makefile.am's purpose is to build the beetest_ test_ program.
#
# Copyright (c) 2001, 2002, 2003 Virtual Unlimited B.V.
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

AUTOMAKE_OPTIONS = gnu no-dependencies

INCLUDES = -I$(top_srcdir)
LDADD = $(top_builddir)/libbeecrypt.la

TESTS = testmd5 testsha1 testsha256 testhmacmd5 testhmacsha1 testmp testrsa testdldp

check_PROGRAMS = testmd5 testsha1 testsha256 testhmacmd5 testhmacsha1 testmp testrsa testdldp

testmd5_SOURCES = testmd5.c

testsha1_SOURCES = testsha1.c

testsha256_SOURCES = testsha256.c

testhmacmd5_SOURCES = testhmacmd5.c

testhmacsha1_SOURCES = testhmacsha1.c

testmp_SOURCES = testmp.c

testrsa_SOURCES = testrsa.c

testdldp_SOURCES = testdldp.c

EXTRA_PROGRAMS = benchme benchhf benchbc

benchme_SOURCES = benchme.c

benchhf_SOURCES = benchhf.c

benchbc_SOURCES = benchbc.c
