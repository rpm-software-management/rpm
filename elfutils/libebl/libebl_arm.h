/* Interface for libebl_arm module.
   Copyright (C) 2002 Red Hat, Inc.

   This program is Open Source software; you can redistribute it and/or
   modify it under the terms of the Open Software License version 1.0 as
   published by the Open Source Initiative.

   You should have received a copy of the Open Software License along
   with this program; if not, you may obtain a copy of the Open Software
   License version 1.0 from http://www.opensource.org/licenses/osl.php or
   by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
   3001 King Ranch Road, Ukiah, CA 95482.   */

#ifndef _LIBEBL_ARM_H
#define _LIBEBL_ARM_H 1

#include <libeblP.h>


/* Constructor.  */
extern int arm_init (Elf *elf, GElf_Half machine, Ebl *eh, size_t ehlen);

/* Destructor.  */
extern void arm_destr (Ebl *bh);


/* Function to get relocation type name.  */
extern const char *arm_reloc_type_name (int type, char *buf, size_t len);

/* Check relocation type.  */
extern bool arm_reloc_type_check (int type);

#endif	/* libebl_arm.h */
