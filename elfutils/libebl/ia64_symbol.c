/* IA-64 specific symbolic name handling.
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

#include <libebl_ia64.h>


/* Return of the backend.  */
const char *
ia64_backend_name (void)
{
  return "ia64";
}


/* Relocation mapping table.  */
static const char *reloc_map_table[] =
  {
    [R_IA64_NONE] = "R_IA64_NONE",
    [R_IA64_IMM14] = "R_IA64_IMM14",
    [R_IA64_IMM22] = "R_IA64_IMM22",
    [R_IA64_IMM64] = "R_IA64_IMM64",
    [R_IA64_DIR32MSB] = "R_IA64_DIR32MSB",
    [R_IA64_DIR32LSB] = "R_IA64_DIR32LSB",
    [R_IA64_DIR64MSB] = "R_IA64_DIR64MSB",
    [R_IA64_DIR64LSB] = "R_IA64_DIR64LSB",
    [R_IA64_GPREL22] = "R_IA64_GPREL22",
    [R_IA64_GPREL64I] = "R_IA64_GPREL64I",
    [R_IA64_GPREL32MSB] = "R_IA64_GPREL32MSB",
    [R_IA64_GPREL32LSB] = "R_IA64_GPREL32LSB",
    [R_IA64_GPREL64MSB] = "R_IA64_GPREL64MSB",
    [R_IA64_GPREL64LSB] = "R_IA64_GPREL64LSB",
    [R_IA64_LTOFF22] = "R_IA64_LTOFF22",
    [R_IA64_LTOFF64I] = "R_IA64_LTOFF64I",
    [R_IA64_PLTOFF22] = "R_IA64_PLTOFF22",
    [R_IA64_PLTOFF64I] = "R_IA64_PLTOFF64I",
    [R_IA64_PLTOFF64MSB] = "R_IA64_PLTOFF64MSB",
    [R_IA64_PLTOFF64LSB] = "R_IA64_PLTOFF64LSB",
    [R_IA64_FPTR64I] = "R_IA64_FPTR64I",
    [R_IA64_FPTR32MSB] = "R_IA64_FPTR32MSB",
    [R_IA64_FPTR32LSB] = "R_IA64_FPTR32LSB",
    [R_IA64_FPTR64MSB] = "R_IA64_FPTR64MSB",
    [R_IA64_FPTR64LSB] = "R_IA64_FPTR64LSB",
    [R_IA64_PCREL60B] = "R_IA64_PCREL60B",
    [R_IA64_PCREL21B] = "R_IA64_PCREL21B",
    [R_IA64_PCREL21M] = "R_IA64_PCREL21M",
    [R_IA64_PCREL21F] = "R_IA64_PCREL21F",
    [R_IA64_PCREL32MSB] = "R_IA64_PCREL32MSB",
    [R_IA64_PCREL32LSB] = "R_IA64_PCREL32LSB",
    [R_IA64_PCREL64MSB] = "R_IA64_PCREL64MSB",
    [R_IA64_PCREL64LSB] = "R_IA64_PCREL64LSB",
    [R_IA64_LTOFF_FPTR22] = "R_IA64_LTOFF_FPTR22",
    [R_IA64_LTOFF_FPTR64I] = "R_IA64_LTOFF_FPTR64I",
    [R_IA64_LTOFF_FPTR32MSB] = "R_IA64_LTOFF_FPTR32MSB",
    [R_IA64_LTOFF_FPTR32LSB] = "R_IA64_LTOFF_FPTR32LSB",
    [R_IA64_LTOFF_FPTR64MSB] = "R_IA64_LTOFF_FPTR64MSB",
    [R_IA64_LTOFF_FPTR64LSB] = "R_IA64_LTOFF_FPTR64LSB",
    [R_IA64_SEGREL32MSB] = "R_IA64_SEGREL32MSB",
    [R_IA64_SEGREL32LSB] = "R_IA64_SEGREL32LSB",
    [R_IA64_SEGREL64MSB] = "R_IA64_SEGREL64MSB",
    [R_IA64_SEGREL64LSB] = "R_IA64_SEGREL64LSB",
    [R_IA64_SECREL32MSB] = "R_IA64_SECREL32MSB",
    [R_IA64_SECREL32LSB] = "R_IA64_SECREL32LSB",
    [R_IA64_SECREL64MSB] = "R_IA64_SECREL64MSB",
    [R_IA64_SECREL64LSB] = "R_IA64_SECREL64LSB",
    [R_IA64_REL32MSB] = "R_IA64_REL32MSB",
    [R_IA64_REL32LSB] = "R_IA64_REL32LSB",
    [R_IA64_REL64MSB] = "R_IA64_REL64MSB",
    [R_IA64_REL64LSB] = "R_IA64_REL64LSB",
    [R_IA64_LTV32MSB] = "R_IA64_LTV32MSB",
    [R_IA64_LTV32LSB] = "R_IA64_LTV32LSB",
    [R_IA64_LTV64MSB] = "R_IA64_LTV64MSB",
    [R_IA64_LTV64LSB] = "R_IA64_LTV64LSB",
    [R_IA64_PCREL21BI] = "R_IA64_PCREL21BI",
    [R_IA64_PCREL22] = "R_IA64_PCREL22",
    [R_IA64_PCREL64I] = "R_IA64_PCREL64I",
    [R_IA64_IPLTMSB] = "R_IA64_IPLTMSB",
    [R_IA64_IPLTLSB] = "R_IA64_IPLTLSB",
    [R_IA64_COPY] = "R_IA64_COPY",
    [R_IA64_SUB] = "R_IA64_SUB",
    [R_IA64_LTOFF22X] = "R_IA64_LTOFF22X",
    [R_IA64_LDXMOV] = "R_IA64_LDXMOV",
    [R_IA64_TPREL14] = "R_IA64_TPREL14",
    [R_IA64_TPREL22] = "R_IA64_TPREL22",
    [R_IA64_TPREL64I] = "R_IA64_TPREL64I",
    [R_IA64_TPREL64MSB] = "R_IA64_TPREL64MSB",
    [R_IA64_TPREL64LSB] = "R_IA64_TPREL64LSB",
    [R_IA64_LTOFF_TPREL22] = "R_IA64_LTOFF_TPREL22",
    [R_IA64_DTPMOD64MSB] = "R_IA64_DTPMOD64MSB",
    [R_IA64_DTPMOD64LSB] = "R_IA64_DTPMOD64LSB",
    [R_IA64_LTOFF_DTPMOD22] = "R_IA64_LTOFF_DTPMOD22",
    [R_IA64_DTPREL14] = "R_IA64_DTPREL14",
    [R_IA64_DTPREL22] = "R_IA64_DTPREL22",
    [R_IA64_DTPREL64I] = "R_IA64_DTPREL64I",
    [R_IA64_DTPREL32MSB] = "R_IA64_DTPREL32MSB",
    [R_IA64_DTPREL32LSB] = "R_IA64_DTPREL32LSB",
    [R_IA64_DTPREL64MSB] = "R_IA64_DTPREL64MSB",
    [R_IA64_DTPREL64LSB] = "R_IA64_DTPREL64LSB",
    [R_IA64_LTOFF_DTPREL22] = "R_IA64_LTOFF_DTPREL22"
  };


/* Determine relocation type string for IA-64.  */
const char *
ia64_reloc_type_name (int type, char *buf, size_t len)
{
  if (type < 0
      || type >= sizeof (reloc_map_table) / sizeof (reloc_map_table[0]))
    return NULL;

  return reloc_map_table[type];
}


/* Check for correct relocation type.  */
bool
ia64_reloc_type_check (int type)
{
  return (type >= R_IA64_NONE
	  && type < sizeof (reloc_map_table) / sizeof (reloc_map_table[0])
	  && reloc_map_table[type] != NULL) ? true : false;
}
