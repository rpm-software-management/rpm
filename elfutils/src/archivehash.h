/* Copyright (C) 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#ifndef ARCHIVEHASH_H
#define ARCHIVEHASH_H	1

/* Definitions for the symbol hash table.  */
#define TYPE Elf_Arsym
#define HASHFCT(str, len) elf_hash (str)
#define HASHTYPE unsigned long int
#define COMPARE(a, b) strcmp ((a)->as_name, (b)->as_name)
#define CLASS static
#define PREFIX arsym_tab_
#define STORE_POINTER 1
#define INSERT_HASH 1
#include <fixedsizehash.h>

#endif	/* archivehash.h */
