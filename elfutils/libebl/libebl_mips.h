/* Interface for libebl_mips module.
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

#ifndef _LIBEBL_MIPS_H
#define _LIBEBL_MIPS_H 1

#include <libeblP.h>


/* Constructor.  */
extern int mips_init (Elf *elf, GElf_Half machine, Ebl *eh, size_t ehlen);

/* Destructor.  */
extern void mips_destr (Ebl *bh);


/* Function to get relocation type name.  */
extern const char *mips_reloc_type_name (int type, char *buf, size_t len);

/* Function to get segment type name.  */
extern const char *mips_segment_type_name (int type, char *buf, size_t len);

/* Function to get setion type name.  */
extern const char *mips_section_type_name (int type, char *buf, size_t len);

/* Function to get machine flag name.  */
extern const char *mips_machine_flag_name (Elf64_Word *flags);

/* Function to get dynamic tag name.  */
extern const char *mips_dynamic_tag_name (int64_t tag, char *buf, size_t len);

#endif	/* libebl_mips.h */
