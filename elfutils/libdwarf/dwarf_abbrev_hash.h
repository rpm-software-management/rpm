/* Hash table for DWARF .debug_abbrev section content.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _DWARF_ABBREV_HASH_H
#define _DWARF_ABBREV_HASH_H	1

#define NAME Dwarf_Abbrev_Hash
#define TYPE Dwarf_Abbrev
#define COMPARE(a, b) (0)

#include <dynamicsizehash.h>

#endif	/* dwarf_abbrev_hash.h */
