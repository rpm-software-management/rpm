/* Implementation of hash table for DWARF .debug_abbrev section content.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define NO_UNDEF
#include "libdwP.h"

#define next_prime __libdwarf_next_prime
extern size_t next_prime (size_t) attribute_hidden;

#include <dynamicsizehash.c>

#undef next_prime
#define next_prime attribute_hidden __libdwarf_next_prime
#include "../lib/next_prime.c"
