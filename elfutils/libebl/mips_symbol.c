/* MIPS specific symbolic name handling.
   Copyright (C) 2000, 2001, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <elf.h>
#include <stddef.h>

#include <libebl_mips.h>


/* Return of the backend.  */
const char *
mips_backend_name (void)
{
  return "mips";
}


/* Determine relocation type string for MIPS.  */
const char *
mips_reloc_type_name (int type, char *buf, size_t len)
{
  static const char *map_table[] =
  {
    [R_MIPS_NONE] = "MIPS_NONE",
    [R_MIPS_16] = "MIPS_16",
    [R_MIPS_32] = "MIPS_32",
    [R_MIPS_REL32] = "MIPS_REL32",
    [R_MIPS_26] = "MIPS_26",
    [R_MIPS_HI16] = "MIPS_HI16",
    [R_MIPS_LO16] = "MIPS_LO16",
    [R_MIPS_GPREL16] = "MIPS_GPREL16",
    [R_MIPS_LITERAL] = "MIPS_LITERAL",
    [R_MIPS_GOT16] = "MIPS_GOT16",
    [R_MIPS_PC16] = "MIPS_PC16",
    [R_MIPS_CALL16] = "MIPS_CALL16",
    [R_MIPS_GPREL32] = "MIPS_GPREL32",
    [R_MIPS_SHIFT5] = "MIPS_SHIFT5",
    [R_MIPS_SHIFT6] = "MIPS_SHIFT6",
    [R_MIPS_64] = "MIPS_64",
    [R_MIPS_GOT_DISP] = "MIPS_GOT_DISP",
    [R_MIPS_GOT_PAGE] = "MIPS_GOT_PAGE",
    [R_MIPS_GOT_OFST] = "MIPS_GOT_OFST",
    [R_MIPS_GOT_HI16] = "MIPS_GOT_HI16",
    [R_MIPS_GOT_LO16] = "MIPS_GOT_LO16",
    [R_MIPS_SUB] = "MIPS_SUB",
    [R_MIPS_INSERT_A] = "MIPS_INSERT_A",
    [R_MIPS_INSERT_B] = "MIPS_INSERT_B",
    [R_MIPS_DELETE] = "MIPS_DELETE",
    [R_MIPS_HIGHER] = "MIPS_HIGHER",
    [R_MIPS_HIGHEST] = "MIPS_HIGHEST",
    [R_MIPS_CALL_HI16] = "MIPS_CALL_HI16",
    [R_MIPS_CALL_LO16] = "MIPS_CALL_LO16",
    [R_MIPS_SCN_DISP] = "MIPS_SCN_DISP",
    [R_MIPS_REL16] = "MIPS_REL16",
    [R_MIPS_ADD_IMMEDIATE] = "MIPS_ADD_IMMEDIATE",
    [R_MIPS_PJUMP] = "MIPS_PJUMP",
    [R_MIPS_RELGOT] = "MIPS_RELGOT",
    [R_MIPS_JALR] = "MIPS_JALR"
  };

  if (type < 0 || type >= R_MIPS_NUM)
    return NULL;

  return map_table[type];
}


const char *
mips_segment_type_name (int type, char *buf, size_t len)
{
  static const struct
  {
    int type;
    const char *str;
  } mips_segments[] =
    {
      { PT_MIPS_REGINFO, "MIPS_REGINFO" },
      { PT_MIPS_RTPROC, "MIPS_RTPROC" },
      { PT_MIPS_OPTIONS, "MIPS_OPTIONS" }
    };
#define nsegments (sizeof (mips_segments) / sizeof (mips_segments[0]))
  int cnt;

  for (cnt = 0; cnt < nsegments; ++cnt)
    if (type == mips_segments[cnt].type)
      return mips_segments[cnt].str;

  /* We don't know the segment type.  */
  return NULL;
}


const char *
mips_section_type_name (int type, char *buf, size_t len)
{
  static const struct
  {
    int type;
    const char *str;
  } mips_sections[] =
    {
      { SHT_MIPS_LIBLIST, "MIPS_LIBLIST" },
      { SHT_MIPS_MSYM, "MIPS_MSYM" },
      { SHT_MIPS_CONFLICT, "MIPS_CONFLICT" },
      { SHT_MIPS_GPTAB, "MIPS_GPTAB" },
      { SHT_MIPS_UCODE, "MIPS_UCODE" },
      { SHT_MIPS_DEBUG, "MIPS_DEBUG" },
      { SHT_MIPS_REGINFO, "MIPS_REGINFO" },
      { SHT_MIPS_PACKAGE, "MIPS_PACKAGE" },
      { SHT_MIPS_PACKSYM, "MIPS_PACKSYM" },
      { SHT_MIPS_RELD, "MIPS_RELD" },
      { SHT_MIPS_IFACE, "MIPS_IFACE" },
      { SHT_MIPS_CONTENT, "MIPS_CONTENT" },
      { SHT_MIPS_OPTIONS, "MIPS_OPTIONS" },
      { SHT_MIPS_SHDR, "MIPS_SHDR" },
      { SHT_MIPS_FDESC, "MIPS_FDESC" },
      { SHT_MIPS_EXTSYM, "MIPS_EXTSYM" },
      { SHT_MIPS_DENSE, "MIPS_DENSE" },
      { SHT_MIPS_PDESC, "MIPS_PDESC" },
      { SHT_MIPS_LOCSYM, "MIPS_LOCSYM" },
      { SHT_MIPS_AUXSYM, "MIPS_AUXSYM" },
      { SHT_MIPS_OPTSYM, "MIPS_OPTSYM" },
      { SHT_MIPS_LOCSTR, "MIPS_LOCSTR" },
      { SHT_MIPS_LINE, "MIPS_LINE" },
      { SHT_MIPS_RFDESC, "MIPS_RFDESC" },
      { SHT_MIPS_DELTASYM, "MIPS_DELTASYM" },
      { SHT_MIPS_DELTAINST, "MIPS_DELTAINST" },
      { SHT_MIPS_DELTACLASS, "MIPS_DELTACLASS" },
      { SHT_MIPS_DWARF, "MIPS_DWARF" },
      { SHT_MIPS_DELTADECL, "MIPS_DELTADECL" },
      { SHT_MIPS_SYMBOL_LIB, "MIPS_SYMBOL_LIB" },
      { SHT_MIPS_EVENTS, "MIPS_EVENTS" },
      { SHT_MIPS_TRANSLATE, "MIPS_TRANSLATE" },
      { SHT_MIPS_PIXIE, "MIPS_PIXIE" },
      { SHT_MIPS_XLATE, "MIPS_XLATE" },
      { SHT_MIPS_XLATE_DEBUG, "MIPS_XLATE_DEBUG" },
      { SHT_MIPS_WHIRL, "MIPS_WHIRL" },
      { SHT_MIPS_EH_REGION, "MIPS_EH_REGION" },
      { SHT_MIPS_XLATE_OLD, "MIPS_XLATE_OLD" },
      { SHT_MIPS_PDR_EXCEPTION, "MIPS_PDR_EXCEPTION" }
    };
#define nsections (sizeof (mips_sections) / sizeof (mips_sections[0]))
  int cnt;

  for (cnt = 0; cnt < nsections; ++cnt)
    if (type == mips_sections[cnt].type)
      return mips_sections[cnt].str;

  /* We don't know the section type.  */
  return NULL;
}


const char *
mips_machine_flag_name (Elf64_Word *flags)
{
  static const struct
  {
    int mask;
    int flag;
    const char *str;
  } mips_flags[] =
    {
      { EF_MIPS_NOREORDER, EF_MIPS_NOREORDER, "noreorder" },
      { EF_MIPS_PIC, EF_MIPS_PIC, "pic" },
      { EF_MIPS_CPIC, EF_MIPS_CPIC, "cpic" },
      { EF_MIPS_ABI2, EF_MIPS_ABI2, "abi2" },
      { EF_MIPS_ARCH, E_MIPS_ARCH_1, "mips1" },
      { EF_MIPS_ARCH, E_MIPS_ARCH_2, "mips2" },
      { EF_MIPS_ARCH, E_MIPS_ARCH_3, "mips3" },
      { EF_MIPS_ARCH, E_MIPS_ARCH_4, "mips4" },
      { EF_MIPS_ARCH, E_MIPS_ARCH_5, "mips5" }
    };
#define nflags (sizeof (mips_flags) / sizeof (mips_flags[0]))
  int cnt;

  for (cnt = 0; cnt < nflags; ++cnt)
    if ((*flags & mips_flags[cnt].mask) == mips_flags[cnt].flag)
      {
	*flags &= ~mips_flags[cnt].mask;
	return mips_flags[cnt].str;
      }

  /* We don't know the flag.  */
  return NULL;
}


const char *
mips_dynamic_tag_name (int64_t tag, char *buf, size_t len)
{
  static const struct
  {
    int tag;
    const char *str;
  } mips_dtags[] =
    {
      { DT_MIPS_RLD_VERSION, "MIPS_RLD_VERSION" },
      { DT_MIPS_TIME_STAMP, "MIPS_TIME_STAMP" },
      { DT_MIPS_ICHECKSUM, "MIPS_ICHECKSUM" },
      { DT_MIPS_IVERSION, "MIPS_IVERSION" },
      { DT_MIPS_FLAGS, "MIPS_FLAGS" },
      { DT_MIPS_BASE_ADDRESS, "MIPS_BASE_ADDRESS" },
      { DT_MIPS_MSYM, "MIPS_MSYM" },
      { DT_MIPS_CONFLICT, "MIPS_CONFLICT" },
      { DT_MIPS_LIBLIST, "MIPS_LIBLIST" },
      { DT_MIPS_LOCAL_GOTNO, "MIPS_LOCAL_GOTNO" },
      { DT_MIPS_CONFLICTNO, "MIPS_CONFLICTNO" },
      { DT_MIPS_LIBLISTNO, "MIPS_LIBLISTNO" },
      { DT_MIPS_SYMTABNO, "MIPS_SYMTABNO" },
      { DT_MIPS_UNREFEXTNO, "MIPS_UNREFEXTNO" },
      { DT_MIPS_GOTSYM, "MIPS_GOTSYM" },
      { DT_MIPS_HIPAGENO, "MIPS_HIPAGENO" },
      { DT_MIPS_RLD_MAP, "MIPS_RLD_MAP" },
      { DT_MIPS_DELTA_CLASS, "MIPS_DELTA_CLASS" },
      { DT_MIPS_DELTA_CLASS_NO, "MIPS_DELTA_CLASS_NO" },
      { DT_MIPS_DELTA_INSTANCE, "MIPS_DELTA_INSTANCE" },
      { DT_MIPS_DELTA_INSTANCE_NO, "MIPS_DELTA_INSTANCE_NO" },
      { DT_MIPS_DELTA_RELOC, "MIPS_DELTA_RELOC" },
      { DT_MIPS_DELTA_RELOC_NO, "MIPS_DELTA_RELOC_NO" },
      { DT_MIPS_DELTA_SYM, "MIPS_DELTA_SYM" },
      { DT_MIPS_DELTA_SYM_NO, "MIPS_DELTA_SYM_NO" },
      { DT_MIPS_DELTA_CLASSSYM, "MIPS_DELTA_CLASSSYM" },
      { DT_MIPS_DELTA_CLASSSYM_NO, "MIPS_DELTA_CLASSSYM_NO" },
      { DT_MIPS_CXX_FLAGS, "MIPS_CXX_FLAGS" },
      { DT_MIPS_PIXIE_INIT, "MIPS_PIXIE_INIT" },
      { DT_MIPS_SYMBOL_LIB, "MIPS_SYMBOL_LIB" },
      { DT_MIPS_LOCALPAGE_GOTIDX, "MIPS_LOCALPAGE_GOTIDX" },
      { DT_MIPS_LOCAL_GOTIDX, "MIPS_LOCAL_GOTIDX" },
      { DT_MIPS_HIDDEN_GOTIDX, "MIPS_HIDDEN_GOTIDX" },
      { DT_MIPS_PROTECTED_GOTIDX, "MIPS_PROTECTED_GOTIDX" },
      { DT_MIPS_OPTIONS, "MIPS_OPTIONS" },
      { DT_MIPS_INTERFACE, "MIPS_INTERFACE" },
      { DT_MIPS_DYNSTR_ALIGN, "MIPS_DYNSTR_ALIGN" },
      { DT_MIPS_INTERFACE_SIZE, "MIPS_INTERFACE_SIZE" },
      { DT_MIPS_RLD_TEXT_RESOLVE_ADDR, "MIPS_RLD_TEXT_RESOLVE_ADDR" },
      { DT_MIPS_PERF_SUFFIX, "MIPS_PERF_SUFFIX" },
      { DT_MIPS_COMPACT_SIZE, "MIPS_COMPACT_SIZE" },
      { DT_MIPS_GP_VALUE, "MIPS_GP_VALUE" },
      { DT_MIPS_AUX_DYNAMIC, "MIPS_AUX_DYNAMIC" },
    };
#define ndtags (sizeof (mips_dtags) / sizeof (mips_dtags[0]))
  int cnt;

  for (cnt = 0; cnt < ndtags; ++cnt)
    if (tag == mips_dtags[cnt].tag)
      return mips_dtags[cnt].str;

  /* We don't know this dynamic tag.  */
  return NULL;
}
