/* Arm specific symbolic name handling.
   Copyright (C) 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#include <elf.h>
#include <stddef.h>

#include <libebl_arm.h>


/* Return of the backend.  */
const char *
arm_backend_name (void)
{
  return "arm";
}


/* Relocation mapping table.  */
static const char *reloc_map_table[] =
  {
    [R_ARM_NONE] = "R_ARM_NONE",
    [R_ARM_PC24] = "R_ARM_PC24",
    [R_ARM_ABS32] = "R_ARM_ABS32",
    [R_ARM_REL32] = "R_ARM_REL32",
    [R_ARM_PC13] = "R_ARM_PC13",
    [R_ARM_ABS16] = "R_ARM_ABS16",
    [R_ARM_ABS12] = "R_ARM_ABS12",
    [R_ARM_THM_ABS5] = "R_ARM_THM_ABS5",
    [R_ARM_ABS8] = "R_ARM_ABS8",
    [R_ARM_SBREL32] = "R_ARM_SBREL32",
    [R_ARM_THM_PC22] = "R_ARM_THM_PC22",
    [R_ARM_THM_PC8] = "R_ARM_THM_PC8",
    [R_ARM_AMP_VCALL9] = "R_ARM_AMP_VCALL9",
    [R_ARM_SWI24] = "R_ARM_SWI24",
    [R_ARM_THM_SWI8] = "R_ARM_THM_SWI8",
    [R_ARM_XPC25] = "R_ARM_XPC25",
    [R_ARM_THM_XPC22] = "R_ARM_THM_XPC22",
    [R_ARM_COPY] = "R_ARM_COPY",
    [R_ARM_GLOB_DAT] = "R_ARM_GLOB_DAT",
    [R_ARM_JUMP_SLOT] = "R_ARM_JUMP_SLOT",
    [R_ARM_RELATIVE] = "R_ARM_RELATIVE",
    [R_ARM_GOTOFF] = "R_ARM_GOTOFF",
    [R_ARM_GOTPC] = "R_ARM_GOTPC",
    [R_ARM_GOT32] = "R_ARM_GOT32",
    [R_ARM_PLT32] = "R_ARM_PLT32",
    [R_ARM_ALU_PCREL_7_0] = "R_ARM_ALU_PCREL_7_0",
    [R_ARM_ALU_PCREL_15_8] = "R_ARM_ALU_PCREL_15_8",
    [R_ARM_ALU_PCREL_23_15] = "R_ARM_ALU_PCREL_23_15",
    [R_ARM_LDR_SBREL_11_0] = "R_ARM_LDR_SBREL_11_0",
    [R_ARM_ALU_SBREL_19_12] = "R_ARM_ALU_SBREL_19_12",
    [R_ARM_ALU_SBREL_27_20] = "R_ARM_ALU_SBREL_27_20"
  };

static const char *reloc_map_table2[] =
  {
    [R_ARM_GNU_VTENTRY] = "R_ARM_GNU_VTENTRY",
    [R_ARM_GNU_VTINHERIT] = "R_ARM_GNU_VTINHERIT",
    [R_ARM_THM_PC11] = "R_ARM_THM_PC11",
    [R_ARM_THM_PC9] = "R_ARM_THM_PC9"
  };

static const char *reloc_map_table3[] =
  {
    [R_ARM_RXPC25] = "R_ARM_RXPC25",
    [R_ARM_RSBREL32] = "R_ARM_RSBREL32",
    [R_ARM_THM_RPC22] = "R_ARM_THM_RPC22",
    [R_ARM_RREL32] = "R_ARM_RREL32",
    [R_ARM_RABS22] = "R_ARM_RABS22",
    [R_ARM_RPC24] = "R_ARM_RPC24",
    [R_ARM_RBASE] = "R_ARM_RBASE"
  };


/* Determine relocation type string for Alpha.  */
const char *
arm_reloc_type_name (int type, char *buf, size_t len)
{
  if (type >= R_ARM_NONE && type <= R_ARM_ALU_SBREL_27_20)
    return reloc_map_table[type];

  if (type >= R_ARM_GNU_VTENTRY && type <= R_ARM_THM_PC9)
    return reloc_map_table2[type - R_ARM_GNU_VTENTRY];

  if (type >= R_ARM_RXPC25 && type <= R_ARM_RBASE)
    return reloc_map_table3[type - R_ARM_RXPC25];

  return NULL;
}


/* Check for correct relocation type.  */
bool
arm_reloc_type_check (int type)
{
  return ((type >= R_ARM_NONE && type <= R_ARM_ALU_SBREL_27_20)
	  || (type >= R_ARM_GNU_VTENTRY && type <= R_ARM_THM_PC9)
	  || (type >= R_ARM_RXPC25 && type <= R_ARM_RBASE)) ? true : false;
}
