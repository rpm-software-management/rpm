/* Interface for libebl_i386 module.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.

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

#ifndef _LIBEBL_I386_H
#define _LIBEBL_I386_H 1

#include <libeblP.h>


/* Constructor.  */
extern int i386_init (Elf *elf, GElf_Half machine, Ebl *eh, size_t ehlen);

/* Destructor.  */
extern void i386_destr (Ebl *bh);


/* Function to get relocation type name.  */
extern const char *i386_reloc_type_name (int type, char *buf, size_t len);

/* Check relocation type.  */
extern bool i386_reloc_type_check (int type);

#endif	/* libebl_i386.h */
