/* Interface for libebl_sparc module.
   Copyright (C) 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _LIBEBL_SPARC_H
#define _LIBEBL_SPARC_H 1

#include <libeblP.h>


/* Constructor.  */
extern int sparc_init (Elf *elf, GElf_Half machine, Ebl *eh, size_t ehlen);

/* Destructor.  */
extern void sparc_destr (Ebl *bh);


/* Function to get relocation type name.  */
extern const char *sparc_reloc_type_name (int type, char *buf, size_t len);

/* Check relocation type.  */
extern bool sparc_reloc_type_check (int type);

/* Code note handling.  */
extern bool sparc_core_note (const char *name, uint32_t type, uint32_t descsz,
			     const char *desc);

#endif	/* libebl_sparc.h */
