/* SPARC specific symbolic name handling.
   Copyright (C) 2002, 2003 Red Hat, Inc.
   Written by Jakub Jelinek <jakub@redhat.com>, 2002.

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

#include <libebl_sparc.h>


/* Return of the backend.  */
const char *
sparc_backend_name (void)
{
  return "sparc";
}


/* Relocation mapping table.  */
static const char *reloc_map_table[] =
  {
    [R_SPARC_NONE] = "R_SPARC_NONE",
    [R_SPARC_8] = "R_SPARC_8",
    [R_SPARC_16] = "R_SPARC_16",
    [R_SPARC_32] = "R_SPARC_32",
    [R_SPARC_DISP8] = "R_SPARC_DISP8",
    [R_SPARC_DISP16] = "R_SPARC_DISP16",
    [R_SPARC_DISP32] = "R_SPARC_DISP32",
    [R_SPARC_WDISP30] = "R_SPARC_WDISP30",
    [R_SPARC_WDISP22] = "R_SPARC_WDISP22",
    [R_SPARC_HI22] = "R_SPARC_HI22",
    [R_SPARC_22] = "R_SPARC_22",
    [R_SPARC_13] = "R_SPARC_13",
    [R_SPARC_LO10] = "R_SPARC_LO10",
    [R_SPARC_GOT10] = "R_SPARC_GOT10",
    [R_SPARC_GOT13] = "R_SPARC_GOT13",
    [R_SPARC_GOT22] = "R_SPARC_GOT22",
    [R_SPARC_PC10] = "R_SPARC_PC10",
    [R_SPARC_PC22] = "R_SPARC_PC22",
    [R_SPARC_WPLT30] = "R_SPARC_WPLT30",
    [R_SPARC_COPY] = "R_SPARC_COPY",
    [R_SPARC_GLOB_DAT] = "R_SPARC_GLOB_DAT",
    [R_SPARC_JMP_SLOT] = "R_SPARC_JMP_SLOT",
    [R_SPARC_RELATIVE] = "R_SPARC_RELATIVE",
    [R_SPARC_UA32] = "R_SPARC_UA32",
    [R_SPARC_PLT32] = "R_SPARC_PLT32",
    [R_SPARC_HIPLT22] = "R_SPARC_HIPLT22",
    [R_SPARC_LOPLT10] = "R_SPARC_LOPLT10",
    [R_SPARC_PCPLT32] = "R_SPARC_PCPLT32",
    [R_SPARC_PCPLT22] = "R_SPARC_PCPLT22",
    [R_SPARC_PCPLT10] = "R_SPARC_PCPLT10",
    [R_SPARC_10] = "R_SPARC_10",
    [R_SPARC_11] = "R_SPARC_11",
    [R_SPARC_64] = "R_SPARC_64",
    [R_SPARC_OLO10] = "R_SPARC_OLO10",
    [R_SPARC_HH22] = "R_SPARC_HH22",
    [R_SPARC_HM10] = "R_SPARC_HM10",
    [R_SPARC_LM22] = "R_SPARC_LM22",
    [R_SPARC_PC_HH22] = "R_SPARC_PC_HH22",
    [R_SPARC_PC_HM10] = "R_SPARC_PC_HM10",
    [R_SPARC_PC_LM22] = "R_SPARC_PC_LM22",
    [R_SPARC_WDISP16] = "R_SPARC_WDISP16",
    [R_SPARC_WDISP19] = "R_SPARC_WDISP19",
    [R_SPARC_7] = "R_SPARC_7",
    [R_SPARC_5] = "R_SPARC_5",
    [R_SPARC_6] = "R_SPARC_6",
    [R_SPARC_DISP64] = "R_SPARC_DISP64",
    [R_SPARC_PLT64] = "R_SPARC_PLT64",
    [R_SPARC_HIX22] = "R_SPARC_HIX22",
    [R_SPARC_LOX10] = "R_SPARC_LOX10",
    [R_SPARC_H44] = "R_SPARC_H44",
    [R_SPARC_M44] = "R_SPARC_M44",
    [R_SPARC_L44] = "R_SPARC_L44",
    [R_SPARC_REGISTER] = "R_SPARC_REGISTER",
    [R_SPARC_UA64] = "R_SPARC_UA64",
    [R_SPARC_UA16] = "R_SPARC_UA16",
    [R_SPARC_TLS_GD_HI22] = "R_SPARC_TLS_GD_HI22",
    [R_SPARC_TLS_GD_LO10] = "R_SPARC_TLS_GD_LO10",
    [R_SPARC_TLS_GD_ADD] = "R_SPARC_TLS_GD_ADD",
    [R_SPARC_TLS_GD_CALL] = "R_SPARC_TLS_GD_CALL",
    [R_SPARC_TLS_LDM_HI22] = "R_SPARC_TLS_LDM_HI22",
    [R_SPARC_TLS_LDM_LO10] = "R_SPARC_TLS_LDM_LO10",
    [R_SPARC_TLS_LDM_ADD] = "R_SPARC_TLS_LDM_ADD",
    [R_SPARC_TLS_LDM_CALL] = "R_SPARC_TLS_LDM_CALL",
    [R_SPARC_TLS_LDO_HIX22] = "R_SPARC_TLS_LDO_HIX22",
    [R_SPARC_TLS_LDO_LOX10] = "R_SPARC_TLS_LDO_LOX10",
    [R_SPARC_TLS_LDO_ADD] = "R_SPARC_TLS_LDO_ADD",
    [R_SPARC_TLS_IE_HI22] = "R_SPARC_TLS_IE_HI22",
    [R_SPARC_TLS_IE_LO10] = "R_SPARC_TLS_IE_LO10",
    [R_SPARC_TLS_IE_LD] = "R_SPARC_TLS_IE_LD",
    [R_SPARC_TLS_IE_LDX] = "R_SPARC_TLS_IE_LDX",
    [R_SPARC_TLS_IE_ADD] = "R_SPARC_TLS_IE_ADD",
    [R_SPARC_TLS_LE_HIX22] = "R_SPARC_TLS_LE_HIX22",
    [R_SPARC_TLS_LE_LOX10] = "R_SPARC_TLS_LE_LOX10",
    [R_SPARC_TLS_DTPMOD32] = "R_SPARC_TLS_DTPMOD32",
    [R_SPARC_TLS_DTPMOD64] = "R_SPARC_TLS_DTPMOD64",
    [R_SPARC_TLS_DTPOFF32] = "R_SPARC_TLS_DTPOFF32",
    [R_SPARC_TLS_DTPOFF64] = "R_SPARC_TLS_DTPOFF64",
    [R_SPARC_TLS_TPOFF32] = "R_SPARC_TLS_TPOFF32",
    [R_SPARC_TLS_TPOFF64] = "R_SPARC_TLS_TPOFF64"
  };


/* Determine relocation type string for sparc.  */
const char *
sparc_reloc_type_name (int type, char *buf, size_t len)
{
  /* High 24 bits of r_type are used for second addend in R_SPARC_OLO10.  */
  if ((type & 0xff) == R_SPARC_OLO10)
    return reloc_map_table[type & 0xff];

  if (type < 0 || type >= R_SPARC_NUM)
    return NULL;

  return reloc_map_table[type];
}


/* Check for correct relocation type.  */
bool
sparc_reloc_type_check (int type)
{
  if ((type & 0xff) == R_SPARC_OLO10)
    return true;
  return (type >= R_SPARC_NONE && type < R_SPARC_NUM
	  && reloc_map_table[type] != NULL) ? true : false;
}
