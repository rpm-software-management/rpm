/* Hash table for DWARF .debug_abbrev section content.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _DWARF_ABBREV_HASH_H
#define _DWARF_ABBREV_HASH_H	1

#define NAME Dwarf_Abbrev_Hash
#define TYPE Dwarf_Abbrev *
#define COMPARE(a, b) (0)

#include <dynamicsizehash.h>

#endif	/* dwarf_abbrev_hash.h */
