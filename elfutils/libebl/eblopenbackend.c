/* Generate ELF backend handle.
   Copyright (C) 2000, 2001, 2002, 2003 Red Hat, Inc.

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

#include <assert.h>
#include <dlfcn.h>
#include <error.h>
#include <gelf.h>
#include <stdlib.h>
#include <string.h>

#include <libeblP.h>


/* This table should contain the complete list of architectures as far
   as the ELF specification is concerned.  */
/* XXX When things are stable replace the string pointers with char
   arrays to avoid relocations.  */
static const struct
{
  const char *dsoname;
  const char *emulation;
  const char *prefix;
  int prefix_len;
  int em;
} machines[] =
{
  { "i386", "elf_i386", "i386", 4, EM_386 },
  { "ia64", "elf_ia64", "ia64", 4, EM_IA_64 },
  { "alpha", "elf_alpha", "alpha", 5, EM_ALPHA },
  { "x86_64", "elf_x86_64", "x86_64", 6, EM_X86_64 },
  { "sh", "elf_sh", "sh", 2, EM_SH },
  { "arm", "ebl_arm", "arm", 3, EM_ARM },
  { "sparc", "elf_sparcv9", "sparc", 5, EM_SPARCV9 },
  { "sparc", "elf_sparc", "sparc", 5, EM_SPARC },
  { "sparc", "elf_sparcv8plus", "sparc", 5, EM_SPARC32PLUS },

  { "m32", "elf_m32", "m32", 3, EM_M32 },
  { "m68k", "elf_m68k", "m68k", 4, EM_68K },
  { "m88k", "elf_m88k", "m88k", 4, EM_88K },
  { "i860", "elf_i860", "i860", 4, EM_860 },
  { "mips", "elf_mips", "mips", 4, EM_MIPS },
  { "s370", "ebl_s370", "s370", 4, EM_S370 },
  { "mips", "elf_mipsel", "mips", 4, EM_MIPS_RS3_LE },
  { "parisc", "elf_parisc", "parisc", 6, EM_PARISC },
  { "vpp500", "elf_vpp500", "vpp500", 5, EM_VPP500 },
  { "sparc", "elf_v8plus", "v8plus", 6, EM_SPARC32PLUS },
  { "i960", "elf_i960", "i960", 4, EM_960 },
  { "ppc", "elf_ppc", "ppc", 3, EM_PPC },
  { "ppc64", "elf_ppc64", "ppc64", 5, EM_PPC64 },
  { "s390", "ebl_s390", "s390", 4, EM_S390 },
  { "v800", "ebl_v800", "v800", 4, EM_V800 },
  { "fr20", "ebl_fr20", "fr20", 4, EM_FR20 },
  { "rh32", "ebl_rh32", "rh32", 4, EM_RH32 },
  { "rce", "ebl_rce", "rce", 3, EM_RCE },
  { "tricore", "elf_tricore", "tricore", 7, EM_TRICORE },
  { "arc", "elf_arc", "arc", 3, EM_ARC },
  { "h8", "elf_h8_300", "h8_300", 6, EM_H8_300 },
  { "h8", "elf_h8_300h", "h8_300h", 6, EM_H8_300H },
  { "h8", "elf_h8s", "h8s", 6, EM_H8S },
  { "h8", "elf_h8_500", "h8_500", 6, EM_H8_500 },
  { "mips_x", "elf_mips_x", "mips_x", 6, EM_MIPS_X },
  { "coldfire", "elf_coldfire", "coldfire", 8, EM_COLDFIRE },
  { "m68k", "elf_68hc12", "68hc12", 6, EM_68HC12 },
  { "mma", "elf_mma", "mma", 3, EM_MMA },
  { "pcp", "elf_pcp", "pcp", 3, EM_PCP },
  { "ncpu", "elf_ncpu", "ncpu", 4, EM_NCPU },
  { "ndr1", "elf_ndr1", "ndr1", 4, EM_NDR1 },
  { "starcore", "elf_starcore", "starcore", 8, EM_STARCORE },
  { "me16", "elf_me16", "em16", 4, EM_ME16 },
  { "st100", "elf_st100", "st100", 5, EM_ST100 },
  { "tinyj", "elf_tinyj", "tinyj", 5, EM_TINYJ },
  { "pdsp", "elf_pdsp", "pdsp", 4, EM_PDSP },
  { "fx66", "elf_fx66", "fx66", 4, EM_FX66 },
  { "st9plus", "elf_st9plus", "st9plus", 7, EM_ST9PLUS },
  { "st7", "elf_st7", "st7", 3, EM_ST7 },
  { "m68k", "elf_68hc16", "68hc16", 6, EM_68HC16 },
  { "m68k", "elf_68hc11", "68hc11", 6, EM_68HC11 },
  { "m68k", "elf_68hc08", "68hc08", 6, EM_68HC08 },
  { "m68k", "elf_68hc05", "68hc05", 6, EM_68HC05 },
  { "svx", "elf_svx", "svx", 3, EM_SVX },
  { "st19", "elf_st19", "st19", 4, EM_ST19 },
  { "vax", "elf_vax", "vax", 3, EM_VAX },
  { "cris", "elf_cris", "cris", 4, EM_CRIS },
  { "javelin", "elf_javelin", "javelin", 7, EM_JAVELIN },
  { "firepath", "elf_firepath", "firepath", 8, EM_FIREPATH },
  { "zsp", "elf_zsp", "zsp", 3, EM_ZSP},
  { "mmix", "elf_mmix", "mmix", 4, EM_MMIX },
  { "hunay", "elf_huany", "huany", 5, EM_HUANY },
  { "prism", "elf_prism", "prism", 5, EM_PRISM },
  { "avr", "elf_avr", "avr", 3, EM_AVR },
  { "fr30", "elf_fr30", "fr30", 4, EM_FR30 },
  { "dv10", "elf_dv10", "dv10", 4, EM_D10V },
  { "dv30", "elf_dv30", "dv30", 4, EM_D30V },
  { "v850", "elf_v850", "v850", 4, EM_V850 },
  { "m32r", "elf_m32r", "m32r", 4, EM_M32R },
  { "mn10300", "elf_mn10300", "mn10300", 7, EM_MN10300 },
  { "mn10200", "elf_mn10200", "mn10200", 7, EM_MN10200 },
  { "pj", "elf_pj", "pj", 2, EM_PJ },
  { "openrisc", "elf_openrisc", "openrisc", 8, EM_OPENRISC },
  { "arc", "elf_arc_a5", "arc_a5", 6, EM_ARC_A5 },
  { "xtensa", "elf_xtensa", "xtensa", 6, EM_XTENSA },
};
#define nmachines (sizeof (machines) / sizeof (machines[0]))


/* Default callbacks.  Mostly they just return the error value.  */
static const char *default_object_type_name (int ignore, char *buf,
					     size_t len);
static const char *default_reloc_type_name (int ignore, char *buf, size_t len);
static bool default_reloc_type_check (int ignore);
static const char *default_segment_type_name (int ignore, char *buf,
					      size_t len);
static const char *default_section_type_name (int ignore, char *buf,
					      size_t len);
static const char *default_section_name (int ignore, int ignore2, char *buf,
					 size_t len);
static const char *default_machine_flag_name (Elf64_Word *ignore);
static bool default_machine_flag_check (Elf64_Word flags);
static const char *default_symbol_type_name (int ignore, char *buf,
					     size_t len);
static const char *default_symbol_binding_name (int ignore, char *buf,
						size_t len);
static const char *default_dynamic_tag_name (int64_t ignore, char *buf,
					     size_t len);
static bool default_dynamic_tag_check (int64_t ignore);
static GElf_Word default_sh_flags_combine (GElf_Word flags1, GElf_Word flags2);
static const char *default_osabi_name (int ignore, char *buf, size_t len);
static void default_destr (struct ebl *ignore);
static const char *default_core_note_type_name (uint32_t, char *buf,
						size_t len);
static const char *default_object_note_type_name (uint32_t, char *buf,
						  size_t len);
static bool default_core_note (const char *name, uint32_t type,
			       uint32_t descsz, const char *desc);
static bool default_object_note (const char *name, uint32_t type,
				 uint32_t descsz, const char *desc);
static bool default_debugscn_p (const char *name);


/* Find an appropriate backend for the file associated with ELF.  */
static Ebl *
openbackend (elf, emulation, machine)
     Elf *elf;
     const char *emulation;
     GElf_Half machine;
{
  Ebl *result;
  int cnt;

  /* First allocate the data structure for the result.  We do this
     here since this assures that the structure is always large
     enough.  */
  result = (Ebl *) calloc (1, sizeof (Ebl));
  if (result == NULL)
    {
      // XXX uncomment
      // __libebl_seterror (ELF_E_NOMEM);
      return NULL;
    }

  /* Fill in the default callbacks.  The initializer for the machine
     specific module can overwrite the values.  */
  result->object_type_name = default_object_type_name;
  result->reloc_type_name = default_reloc_type_name;
  result->reloc_type_check = default_reloc_type_check;
  result->segment_type_name = default_segment_type_name;
  result->section_type_name = default_section_type_name;
  result->section_name = default_section_name;
  result->machine_flag_name = default_machine_flag_name;
  result->machine_flag_check = default_machine_flag_check;
  result->symbol_type_name = default_symbol_type_name;
  result->symbol_binding_name = default_symbol_binding_name;
  result->dynamic_tag_name = default_dynamic_tag_name;
  result->dynamic_tag_check = default_dynamic_tag_check;
  result->sh_flags_combine = default_sh_flags_combine;
  result->osabi_name = default_osabi_name;
  result->core_note_type_name = default_core_note_type_name;
  result->object_note_type_name = default_object_note_type_name;
  result->core_note = default_core_note;
  result->object_note = default_object_note;
  result->debugscn_p = default_debugscn_p;
  result->destr = default_destr;

  /* XXX Currently all we do is to look at 'e_machine' value in the
     ELF header.  With an internal mapping table from EM_* value to
     DSO name we try to load the appropriate module to handle this
     binary type.

     Multiple modules for the same machine type are possible and they
     will be tried in sequence.  The lookup process will only stop
     when a module which can handle the machine type is found or all
     available matching modules are tried.  */
  for (cnt = 0; cnt < nmachines; ++cnt)
    if ((emulation != NULL && strcmp (emulation, machines[cnt].emulation) == 0)
	|| (emulation == NULL && machines[cnt].em == machine))
      {
	char dsoname[100];
	void *h;

	/* Well, we know the emulation name now.  */
	result->emulation = machines[cnt].emulation;

	/* Give it a try.  At least the machine type matches.  First
           try to load the module.  */
	strcpy (stpcpy (stpcpy (dsoname, "$ORIGIN/elfutils/libebl_"),
			machines[cnt].dsoname),
		".so");

	h = dlopen (dsoname, RTLD_LAZY);
	if (h != NULL)
	  {
	    /* We managed to load the object.  Now see whether the
	       initialization function likes our file.  */
	    ebl_bhinit_t initp;
	    char symname[machines[cnt].prefix_len + sizeof "_init"];

	    strcpy (mempcpy (symname, machines[cnt].prefix,
			     machines[cnt].prefix_len), "_init");

	    initp = (ebl_bhinit_t) dlsym (h, symname);
	    if (initp != NULL
		&& initp (elf, machine, result, sizeof (Ebl)) == 0)
	      {
		/* We found a module to handle our file.  */
		result->dlhandle = h;
		result->elf = elf;

		/* A few entries are mandatory.  */
		assert (result->name != NULL);
		assert (result->destr != NULL);

		return result;
	      }

	    /* Not the module we need.  */
	    (void) dlclose (h);
	  }

	/* We cannot find a DSO but the emulation/machine ID matches.
	   Return that information.  */
	result->dlhandle = NULL;
	result->elf = elf;
	result->name = machines[cnt].prefix;

	return result;
      }

  /* Nothing matched.  We use only the default callbacks.   */
  result->dlhandle = NULL;
  result->elf = elf;
  result->emulation = "<unknown>";
  result->name = "<unknown>";

  return result;
}


/* Find an appropriate backend for the file associated with ELF.  */
Ebl *
ebl_openbackend (elf)
     Elf *elf;
{
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;

  /* Get the ELF header of the object.  */
  ehdr = gelf_getehdr (elf, &ehdr_mem);
  if (ehdr == NULL)
    {
      // XXX uncomment
      // __libebl_seterror (elf_errno ());
      return NULL;
    }

  return openbackend (elf, NULL, ehdr->e_machine);
}


/* Find backend without underlying ELF file.  */
Ebl *
ebl_openbackend_machine (machine)
     GElf_Half machine;
{
  return openbackend (NULL, NULL, machine);
}


/* Find backend with given emulation name.  */
Ebl *
ebl_openbackend_emulation (const char *emulation)
{
  return openbackend (NULL, emulation, EM_NONE);
}


/* Default callbacks.  Mostly they just return the error value.  */
static const char *
default_object_type_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_reloc_type_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static bool
default_reloc_type_check (int ignore)
{
  return false;
}

static const char *
default_segment_type_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_section_type_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_section_name (int ignore, int ignore2, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_machine_flag_name (Elf64_Word *ignore)
{
  return NULL;
}

static bool
default_machine_flag_check (Elf64_Word flags)
{
  return flags == 0;
}

static const char *
default_symbol_type_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_symbol_binding_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_dynamic_tag_name (int64_t ignore, char *buf, size_t len)
{
  return NULL;
}

static bool
default_dynamic_tag_check (int64_t ignore)
{
  return false;
}

static GElf_Word
default_sh_flags_combine (GElf_Word flags1, GElf_Word flags2)
{
  return SH_FLAGS_COMBINE (flags1, flags2);
}

static void
default_destr (struct ebl *ignore)
{
}

static const char *
default_osabi_name (int ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_core_note_type_name (uint32_t ignore, char *buf, size_t len)
{
  return NULL;
}

static const char *
default_object_note_type_name (uint32_t ignore, char *buf, size_t len)
{
  return NULL;
}

static bool
default_core_note (const char *name, uint32_t type, uint32_t descsz,
		   const char *desc)
{
  return false;
}

static bool
default_object_note (const char *name, uint32_t type, uint32_t descsz,
		     const char *desc)
{
  return false;
}

static bool
default_debugscn_p (const char *name)
{
  /* We know by default only about the DWARF debug sections which have
     fixed names.  */
  static const char *dwarf_scn_names[] =
    {
      /* DWARF 1 */
      ".debug",
      ".line",
      /* GNU DWARF 1 extensions */
      ".debug_srcinfo",
      ".debug_sfnames",
      /* DWARF 1.1 and DWARF 2 */
      ".debug_aranges",
      ".debug_pubnames",
      /* DWARF 2 */
      ".debug_info",
      ".debug_abbrev",
      ".debug_line",
      ".debug_frame",
      ".debug_str",
      ".debug_loc",
      ".debug_macinfo",
      /* DWARF 3 */
      ".debug_ranges",
      /* SGI/MIPS DWARF 2 extensions */
      ".debug_weaknames",
      ".debug_funcnames",
      ".debug_typenames",
      ".debug_varnames"
    };
  const size_t ndwarf_scn_names = (sizeof (dwarf_scn_names)
				   / sizeof (dwarf_scn_names[0]));
  size_t cnt;

  for (cnt = 0; cnt < ndwarf_scn_names; ++cnt)
    if (strcmp (name, dwarf_scn_names[cnt]) == 0)
      return true;

  return false;
}
