## Process this file with automake to create Makefile.in
## Configure input file for elfutils.
##
## Copyright (C) 2002 Red Hat, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2 as
## published by the Free Software Foundation.
##
## his program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software Foundation,
## Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##
DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wshadow
INCLUDES = -I$(srcdir)

LINT = splint

noinst_LTLIBRARIES = libcpu_i386.la

libcpu_i386_la_SOURCES = i386_dis.c

.PSEUDO: lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(GCC_INCLUDE) \
		$(addprefix $(srcdir)/,$(libcpu_la_SOURCES))
