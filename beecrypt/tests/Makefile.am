#
# Makefile.am's purpose is to build the beetest test program.
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

AUTOMAKE_OPTIONS = gnu no-dependencies

INCLUDES = -I$(top_srcdir)
LDADD = $(top_builddir)/libbeecrypt.la

EXTRA_PROGRAMS = beetest openpgp base64bug
 
beetest_SOURCES = beetest.c
beetest_LDFLAGS = -all-static

openpgp_SOURCES = openpgp.c

base64bug_SOURCES = base64bug.c
