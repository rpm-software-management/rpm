/* Generate ELF backend handle.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <error.h>
#include <gelf.h>
#include <ltdl.h>
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
  { "libebl_m32", "elf_m32", "m32", 3, EM_M32 },
  { "libebl_SPARC", "elf_sparc", "sparc", 5, EM_SPARC },
  { "libebl_i386", "elf_i386", "i386", 4, EM_386 },
  { "libebl_m68k", "elf_m68k", "m68k", 4, EM_68K },
  { "libebl_m88k", "elf_m88k", "m88k", 4, EM_88K },
  { "libebl_i860", "elf_i860", "i860", 4, EM_860 },
  { "libebl_mips", "elf_mips", "mips", 4, EM_MIPS },
  { "libebl_s370", "ebl_s370", "s370", 4, EM_S370 },
  { "libebl_mips", "elf_mipsel", "mips", 4, EM_MIPS_RS3_LE },
  { "libebl_parisc", "elf_parisc", "parisc", 6, EM_PARISC },
  { "libebl_vpp500", "elf_vpp500", "vpp500", 5, EM_VPP500 },
  { "libebl_v8plus", "elf_v8plus", "v8plus", 6, EM_SPARC32PLUS },
  { "libebl_i960", "elf_i960", "i960", 4, EM_960 },
  { "libebl_ppc", "elf_ppc", "ppc", 3, EM_PPC },
  { "libebl_ppc64", "elf_ppc64", "ppc64", 5, EM_PPC64 },
  { "libebl_s390", "ebl_s390", "s390", 4, EM_S390 },
  { "libebl_v800", "ebl_v800", "v800", 4, EM_V800 },
  { "libebl_fr20", "ebl_fr20", "fr20", 4, EM_FR20 },
  { "libebl_rh32", "ebl_rh32", "rh32", 4, EM_RH32 },
  { "libebl_rce", "ebl_rce", "rce", 3, EM_RCE },
  { "libebl_arm", "ebl_arm", "arm", 3, EM_ARM },
  { "libebl_sh", "elf_sh", "sh", 2, EM_SH },
  /* XXX Many more missing ... */
};
#define nmachines (sizeof (machines) / sizeof (machines[0]))


/* Initialize the ltdl library.  */
static void
dlinit (void)
{
  if (lt_dlinit () != 0)
    error (EXIT_FAILURE, 0, _("initialization of libltdl failed"));

  /* Make sure we can find our modules.  */
  /* XXX Use the correct path when done.  */
  lt_dladdsearchdir (OBJDIR "/.libs");
}


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


/* Find an appropriate backend for the file associated with ELF.  */
static Ebl *
openbackend (elf, emulation, machine)
     Elf *elf;
     const char *emulation;
     GElf_Half machine;
{
  once_define (static, once);
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

  /* We have to initialized the dynamic loading library.  */
  once_execute (once, dlinit);

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
	/* Give it a try.  At least the machine type matches.  First
           try to load the module.  */
	lt_dlhandle h;

	h = lt_dlopenext (machines[cnt].dsoname);
	if (h != NULL)
	  {
	    /* We managed to load the object.  Now see whether the
	       initialization function likes our file.  */
	    ebl_bhinit_t initp;
	    char symname[machines[cnt].prefix_len + sizeof "_init"];

	    strcpy (mempcpy (symname, machines[cnt].prefix,
			     machines[cnt].prefix_len), "_init");

	    initp = (ebl_bhinit_t) lt_dlsym (h, symname);
	    if (initp != NULL
		&& initp (elf, machine, result, sizeof (Ebl)) == 0)
	      {
		/* We found a module to handle our file.  */
		result->dlhandle = h;
		result->elf = elf;
		result->emulation = machines[cnt].emulation;

		/* A few entries are mandatory.  */
		assert (result->name != NULL);
		assert (result->destr != NULL);

		return result;
	      }

	    /* Not the module we need.  */
	    (void) lt_dlclose (h);
	  }
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
