/* Copyright (C) 2001-2003, 2005, 2007, 2009-2011, 2016, 2017 Red Hat, Inc.
   Written by Alexander Larsson <alexl@redhat.com>, 2002
   Based on code by Jakub Jelinek <jakub@redhat.com>, 2001.
   String/Line table rewriting by Mark Wielaard <mjw@redhat.com>, 2017.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

/* Needed for libelf */
#define _FILE_OFFSET_BITS 64

#include <assert.h>
#include <byteswap.h>
#include <endian.h>
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <popt.h>

#include <gelf.h>
#include <dwarf.h>


/* Unfortunately strtab manipulation functions were only officially added
   to elfutils libdw in 0.167.  Before that there were internal unsupported
   ebl variants.  While libebl.h isn't supported we'll try to use it anyway
   if the elfutils we build against is too old.  */
#include <elfutils/version.h>
#if _ELFUTILS_PREREQ (0, 167)
#include <elfutils/libdwelf.h>
typedef Dwelf_Strent		Strent;
typedef Dwelf_Strtab		Strtab;
#define strtab_init		dwelf_strtab_init
#define strtab_add(X,Y)		dwelf_strtab_add(X,Y)
#define strtab_add_len(X,Y,Z)	dwelf_strtab_add_len(X,Y,Z)
#define strtab_free		dwelf_strtab_free
#define strtab_finalize		dwelf_strtab_finalize
#define strent_offset		dwelf_strent_off
#else
#include <elfutils/libebl.h>
typedef struct Ebl_Strent	Strent;
typedef struct Ebl_Strtab	Strtab;
#define strtab_init		ebl_strtabinit
#define strtab_add(X,Y)		ebl_strtabadd(X,Y,0)
#define strtab_add_len(X,Y,Z)	ebl_strtabadd(X,Y,Z)
#define strtab_free		ebl_strtabfree
#define strtab_finalize		ebl_strtabfinalize
#define strent_offset		ebl_strtaboffset
#endif

#include <search.h>

#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>
#include "tools/hashtab.h"

#define DW_TAG_partial_unit 0x3c
#define DW_FORM_sec_offset 0x17
#define DW_FORM_exprloc 0x18
#define DW_FORM_flag_present 0x19
#define DW_FORM_ref_sig8 0x20

char *base_dir = NULL;
char *dest_dir = NULL;
char *list_file = NULL;
int list_file_fd = -1;
int do_build_id = 0;
int no_recompute_build_id = 0;
char *build_id_seed = NULL;

int show_version = 0;

/* We go over the debug sections in two phases. In phase zero we keep
   track of any needed changes and collect strings, indexes and
   sizes. In phase one we do the actual replacements updating the
   strings, indexes and writing out new debug sections. The following
   keep track of various changes that might be needed. */

/* Whether we need to do any literal string (DW_FORM_string) replacements
   in debug_info. */
static bool need_string_replacement = false;
/* Whether we need to do any updates of the string indexes (DW_FORM_strp)
   in debug_info for string indexes. */
static bool need_strp_update = false;
/* If the debug_line changes size we will need to update the
   DW_AT_stmt_list attributes indexes in the debug_info. */
static bool need_stmt_update = false;

/* Storage for dynamically allocated strings to put into string
   table. Keep together in memory blocks of 16K. */
#define STRMEMSIZE (16 * 1024)
struct strmemblock
{
  struct strmemblock *next;
  char memory[0];
};

/* We keep track of each index in the original string table and the
   associated entry in the new table so we don't insert identical
   strings into the new string table. If constructed correctly the
   original strtab shouldn't contain duplicate strings anyway. Any
   actual identical strings could be deduplicated, but searching for
   and comparing the indexes is much faster than comparing strings
   (and we don't have to construct replacement strings). */
struct stridxentry
{
  uint32_t idx; /* Original index in the string table. */
  Strent *entry; /* Entry in the new table. */
};

/* Storage for new string table entries. Keep together in memory to
   quickly search through them with tsearch. */
#define STRIDXENTRIES ((16 * 1024) / sizeof (struct stridxentry))
struct strentblock
{
  struct strentblock *next;
  struct stridxentry entry[0];
};

/* All data to keep track of the existing and new string table. */
struct strings
{
  Strtab *str_tab;			/* The new string table. */
  char *str_buf;			/* New Elf_Data d_buf. */
  struct strmemblock *blocks;		/* The first strmemblock. */
  struct strmemblock *last_block;	/* The currently used strmemblock. */
  size_t stridx;			/* Next free byte in last block. */
  struct strentblock *entries;		/* The first string index block. */
  struct strentblock *last_entries;	/* The currently used strentblock. */
  size_t entryidx;			/* Next free entry in the last block. */
  void *strent_root;			/* strent binary search tree root. */
};

struct line_table
{
  size_t old_idx;     /* Original offset. */
  size_t new_idx;     /* Offset in new debug_line section. */
  ssize_t size_diff;  /* Difference in (header) size. */
  bool replace_dirs;  /* Whether to replace any dir paths.  */
  bool replace_files; /* Whether to replace any file paths. */

  /* Header fields. */
  uint32_t unit_length;
  uint16_t version;
  uint32_t header_length;
  uint8_t min_instr_len;
  uint8_t max_op_per_instr; /* Only if version >= 4 */
  uint8_t default_is_stmt;
  int8_t line_base;
  uint8_t line_range;
  uint8_t opcode_base;
};

struct debug_lines
{
  struct line_table *table; /* Malloc/Realloced. */
  size_t size;              /* Total number of line_tables.
			       Updated by get_line_table. */
  size_t used;              /* Used number of line_tables.
			       Updated by get_line_table. */
  size_t debug_lines_len;   /* Total size of new debug_line section.
			       updated by edit_dwarf2_line. */
  char *line_buf;           /* New Elf_Data d_buf. */
};

typedef struct
{
  Elf *elf;
  GElf_Ehdr ehdr;
  Elf_Scn **scn;
  const char *filename;
  int lastscn;
  size_t phnum;
  struct strings strings;
  struct debug_lines lines;
  GElf_Shdr shdr[0];
} DSO;

static void
setup_lines (struct debug_lines *lines)
{
  lines->table = NULL;
  lines->size = 0;
  lines->used = 0;
  lines->debug_lines_len = 0;
  lines->line_buf = NULL;
}

static void
destroy_lines (struct debug_lines *lines)
{
  free (lines->table);
  free (lines->line_buf);
}

typedef struct
{
  unsigned char *ptr;
  uint32_t addend;
  int ndx;
} REL;

typedef struct
{
  Elf64_Addr r_offset;
  int ndx;
} LINE_REL;

#define read_uleb128(ptr) ({		\
  unsigned int ret = 0;			\
  unsigned int c;			\
  int shift = 0;			\
  do					\
    {					\
      c = *ptr++;			\
      ret |= (c & 0x7f) << shift;	\
      shift += 7;			\
    } while (c & 0x80);			\
					\
  if (shift >= 35)			\
    ret = UINT_MAX;			\
  ret;					\
})

#define write_uleb128(ptr,val) ({	\
  uint32_t valv = (val);		\
  do					\
    {					\
      unsigned char c = valv & 0x7f;	\
      valv >>= 7;			\
      if (valv)				\
	c |= 0x80;			\
      *ptr++ = c;			\
    }					\
  while (valv);				\
})

static uint16_t (*do_read_16) (unsigned char *ptr);
static uint32_t (*do_read_32) (unsigned char *ptr);
static void (*do_write_16) (unsigned char *ptr, uint16_t val);
static void (*do_write_32) (unsigned char *ptr, uint32_t val);

static int ptr_size;
static int cu_version;

static inline uint16_t
buf_read_ule16 (unsigned char *data)
{
  return data[0] | (data[1] << 8);
}

static inline uint16_t
buf_read_ube16 (unsigned char *data)
{
  return data[1] | (data[0] << 8);
}

static inline uint32_t
buf_read_ule32 (unsigned char *data)
{
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static inline uint32_t
buf_read_ube32 (unsigned char *data)
{
  return data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
}

static const char *
strptr (DSO *dso, int sec, off_t offset)
{
  Elf_Scn *scn;
  Elf_Data *data;

  scn = dso->scn[sec];
  if (offset >= 0 && (GElf_Addr) offset < dso->shdr[sec].sh_size)
    {
      data = NULL;
      while ((data = elf_getdata (scn, data)) != NULL)
	{
	  if (data->d_buf
	      && offset >= data->d_off
	      && offset < data->d_off + data->d_size)
	    return (const char *) data->d_buf + (offset - data->d_off);
	}
    }

  return NULL;
}


#define read_8(ptr) *ptr++

#define read_16(ptr) ({					\
  uint16_t ret = do_read_16 (ptr);			\
  ptr += 2;						\
  ret;							\
})

#define read_32(ptr) ({					\
  uint32_t ret = do_read_32 (ptr);			\
  ptr += 4;						\
  ret;							\
})

REL *relptr, *relend;
int reltype;

#define do_read_32_relocated(ptr) ({			\
  uint32_t dret = do_read_32 (ptr);			\
  if (relptr)						\
    {							\
      while (relptr < relend && relptr->ptr < ptr)	\
	++relptr;					\
      if (relptr < relend && relptr->ptr == ptr)	\
	{						\
	  if (reltype == SHT_REL)			\
	    dret += relptr->addend;			\
	  else						\
	    dret = relptr->addend;			\
	}						\
    }							\
  dret;							\
})

#define read_32_relocated(ptr) ({			\
  uint32_t ret = do_read_32_relocated (ptr);		\
  ptr += 4;						\
  ret;							\
})

static void
dwarf2_write_le16 (unsigned char *p, uint16_t v)
{
  p[0] = v;
  p[1] = v >> 8;
}

static void
dwarf2_write_le32 (unsigned char *p, uint32_t v)
{
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

static void
dwarf2_write_be16 (unsigned char *p, uint16_t v)
{
  p[1] = v;
  p[0] = v >> 8;
}

static void
dwarf2_write_be32 (unsigned char *p, uint32_t v)
{
  p[3] = v;
  p[2] = v >> 8;
  p[1] = v >> 16;
  p[0] = v >> 24;
}

#define write_8(ptr,val) ({	\
  *ptr++ = (val);		\
})

#define write_16(ptr,val) ({	\
  do_write_16 (ptr,val);	\
  ptr += 2;			\
})

#define write_32(ptr,val) ({	\
  do_write_32 (ptr,val);	\
  ptr += 4;			\
})

/* relocated writes can only be called immediately after
   do_read_32_relocated.  ptr must be equal to relptr->ptr (or
   relend). Might just update the addend. So relocations need to be
   updated at the end.  */

static bool rel_updated;

#define do_write_32_relocated(ptr,val) ({ \
  if (relptr && relptr < relend && relptr->ptr == ptr)	\
    {							\
      if (reltype == SHT_REL)				\
	do_write_32 (ptr, val - relptr->addend);	\
      else						\
	{						\
	  relptr->addend = val;				\
	  rel_updated = true;				\
	}						\
    }							\
  else							\
    do_write_32 (ptr,val);				\
})

#define write_32_relocated(ptr,val) ({ \
  do_write_32_relocated (ptr,val);     \
  ptr += 4;			       \
})

typedef struct debug_section
  {
    const char *name;
    unsigned char *data;
    Elf_Data *elf_data;
    size_t size;
    int sec, relsec;
    REL *relbuf;
    REL *relend;
    struct debug_section *next; /* Only happens for COMDAT .debug_macro.  */
  } debug_section;

static debug_section debug_sections[] =
  {
#define DEBUG_INFO	0
#define DEBUG_ABBREV	1
#define DEBUG_LINE	2
#define DEBUG_ARANGES	3
#define DEBUG_PUBNAMES	4
#define DEBUG_PUBTYPES	5
#define DEBUG_MACINFO	6
#define DEBUG_LOC	7
#define DEBUG_STR	8
#define DEBUG_FRAME	9
#define DEBUG_RANGES	10
#define DEBUG_TYPES	11
#define DEBUG_MACRO	12
#define DEBUG_GDB_SCRIPT	13
    { ".debug_info", NULL, NULL, 0, 0, 0 },
    { ".debug_abbrev", NULL, NULL, 0, 0, 0 },
    { ".debug_line", NULL, NULL, 0, 0, 0 },
    { ".debug_aranges", NULL, NULL, 0, 0, 0 },
    { ".debug_pubnames", NULL, NULL, 0, 0, 0 },
    { ".debug_pubtypes", NULL, NULL, 0, 0, 0 },
    { ".debug_macinfo", NULL, NULL, 0, 0, 0 },
    { ".debug_loc", NULL, NULL, 0, 0, 0 },
    { ".debug_str", NULL, NULL, 0, 0, 0 },
    { ".debug_frame", NULL, NULL, 0, 0, 0 },
    { ".debug_ranges", NULL, NULL, 0, 0, 0 },
    { ".debug_types", NULL, NULL, 0, 0, 0 },
    { ".debug_macro", NULL, NULL, 0, 0, 0 },
    { ".debug_gdb_scripts", NULL, NULL, 0, 0, 0 },
    { NULL, NULL, NULL, 0, 0, 0 }
  };

static int
rel_cmp (const void *a, const void *b)
{
  REL *rela = (REL *) a, *relb = (REL *) b;

  if (rela->ptr < relb->ptr)
    return -1;

  if (rela->ptr > relb->ptr)
    return 1;

  return 0;
}

/* Returns a malloced REL array, or NULL when there are no relocations
   for this section.  When there are relocations, will setup relend,
   as the last REL, and reltype, as SHT_REL or SHT_RELA.  */
static void
setup_relbuf (DSO *dso, debug_section *sec, int *reltype)
{
  int ndx, maxndx;
  GElf_Rel rel;
  GElf_Rela rela;
  GElf_Sym sym;
  GElf_Addr base = dso->shdr[sec->sec].sh_addr;
  Elf_Data *symdata = NULL;
  int rtype;
  REL *relbuf;
  Elf_Scn *scn;
  Elf_Data *data;
  int i = sec->relsec;

  /* No relocations, or did we do this already? */
  if (i == 0 || sec->relbuf != NULL)
    {
      relptr = sec->relbuf;
      relend = sec->relend;
      return;
    }

  scn = dso->scn[i];
  data = elf_getdata (scn, NULL);
  assert (data != NULL && data->d_buf != NULL);
  assert (elf_getdata (scn, data) == NULL);
  assert (data->d_off == 0);
  assert (data->d_size == dso->shdr[i].sh_size);
  maxndx = dso->shdr[i].sh_size / dso->shdr[i].sh_entsize;
  relbuf = malloc (maxndx * sizeof (REL));
  *reltype = dso->shdr[i].sh_type;
  if (relbuf == NULL)
    error (1, errno, "%s: Could not allocate memory", dso->filename);

  symdata = elf_getdata (dso->scn[dso->shdr[i].sh_link], NULL);
  assert (symdata != NULL && symdata->d_buf != NULL);
  assert (elf_getdata (dso->scn[dso->shdr[i].sh_link], symdata) == NULL);
  assert (symdata->d_off == 0);
  assert (symdata->d_size == dso->shdr[dso->shdr[i].sh_link].sh_size);

  for (ndx = 0, relend = relbuf; ndx < maxndx; ++ndx)
    {
      if (dso->shdr[i].sh_type == SHT_REL)
	{
	  gelf_getrel (data, ndx, &rel);
	  rela.r_offset = rel.r_offset;
	  rela.r_info = rel.r_info;
	  rela.r_addend = 0;
	}
      else
	gelf_getrela (data, ndx, &rela);
      gelf_getsym (symdata, ELF64_R_SYM (rela.r_info), &sym);
      /* Relocations against section symbols are uninteresting in REL.  */
      if (dso->shdr[i].sh_type == SHT_REL && sym.st_value == 0)
	continue;
      /* Only consider relocations against .debug_str, .debug_line
	 and .debug_abbrev.  */
      if (sym.st_shndx != debug_sections[DEBUG_STR].sec
	  && sym.st_shndx != debug_sections[DEBUG_LINE].sec
	  && sym.st_shndx != debug_sections[DEBUG_ABBREV].sec)
	continue;
      rela.r_addend += sym.st_value;
      rtype = ELF64_R_TYPE (rela.r_info);
      switch (dso->ehdr.e_machine)
	{
	case EM_SPARC:
	case EM_SPARC32PLUS:
	case EM_SPARCV9:
	  if (rtype != R_SPARC_32 && rtype != R_SPARC_UA32)
	    goto fail;
	  break;
	case EM_386:
	  if (rtype != R_386_32)
	    goto fail;
	  break;
	case EM_PPC:
	case EM_PPC64:
	  if (rtype != R_PPC_ADDR32 && rtype != R_PPC_UADDR32)
	    goto fail;
	  break;
	case EM_S390:
	  if (rtype != R_390_32)
	    goto fail;
	  break;
	case EM_IA_64:
	  if (rtype != R_IA64_SECREL32LSB)
	    goto fail;
	  break;
	case EM_X86_64:
	  if (rtype != R_X86_64_32)
	    goto fail;
	  break;
	case EM_ALPHA:
	  if (rtype != R_ALPHA_REFLONG)
	    goto fail;
	  break;
#if defined(EM_AARCH64) && defined(R_AARCH64_ABS32)
	case EM_AARCH64:
	  if (rtype != R_AARCH64_ABS32)
	    goto fail;
	  break;
#endif
	case EM_68K:
	  if (rtype != R_68K_32)
	    goto fail;
	  break;
#if defined(EM_RISCV) && defined(R_RISCV_32)
	case EM_RISCV:
	  if (rtype != R_RISCV_32)
	    goto fail;
	  break;
#endif
	default:
	fail:
	  error (1, 0, "%s: Unhandled relocation %d in %s section",
		 dso->filename, rtype, sec->name);
	}
      relend->ptr = sec->data
	+ (rela.r_offset - base);
      relend->addend = rela.r_addend;
      relend->ndx = ndx;
      ++(relend);
    }
  if (relbuf == relend)
    {
      free (relbuf);
      relbuf = NULL;
      relend = NULL;
    }
  else
    qsort (relbuf, relend - relbuf, sizeof (REL), rel_cmp);

  sec->relbuf = relbuf;
  sec->relend = relend;
  relptr = relbuf;
}

/* Updates SHT_RELA section associated with the given section based on
   the relbuf data. The relbuf data is freed at the end.  */
static void
update_rela_data (DSO *dso, struct debug_section *sec)
{
  Elf_Data *symdata;
  int relsec_ndx = sec->relsec;
  Elf_Data *data = elf_getdata (dso->scn[relsec_ndx], NULL);
  symdata = elf_getdata (dso->scn[dso->shdr[relsec_ndx].sh_link],
			 NULL);

  relptr = sec->relbuf;
  relend = sec->relend;
  while (relptr < relend)
    {
      GElf_Sym sym;
      GElf_Rela rela;
      int ndx = relptr->ndx;

      if (gelf_getrela (data, ndx, &rela) == NULL)
	error (1, 0, "Couldn't get relocation: %s",
	       elf_errmsg (-1));

      if (gelf_getsym (symdata, GELF_R_SYM (rela.r_info),
		       &sym) == NULL)
	error (1, 0, "Couldn't get symbol: %s", elf_errmsg (-1));

      rela.r_addend = relptr->addend - sym.st_value;

      if (gelf_update_rela (data, ndx, &rela) == 0)
	error (1, 0, "Couldn't update relocations: %s",
	       elf_errmsg (-1));

      ++relptr;
    }
  elf_flagdata (data, ELF_C_SET, ELF_F_DIRTY);

  free (sec->relbuf);
}

struct abbrev_attr
  {
    unsigned int attr;
    unsigned int form;
  };

struct abbrev_tag
  {
    unsigned int entry;
    unsigned int tag;
    int nattr;
    struct abbrev_attr attr[0];
  };

static hashval_t
abbrev_hash (const void *p)
{
  struct abbrev_tag *t = (struct abbrev_tag *)p;

  return t->entry;
}

static int
abbrev_eq (const void *p, const void *q)
{
  struct abbrev_tag *t1 = (struct abbrev_tag *)p;
  struct abbrev_tag *t2 = (struct abbrev_tag *)q;

  return t1->entry == t2->entry;
}

static void
abbrev_del (void *p)
{
  free (p);
}

static htab_t
read_abbrev (DSO *dso, unsigned char *ptr)
{
  htab_t h = htab_try_create (50, abbrev_hash, abbrev_eq, abbrev_del);
  unsigned int attr, form;
  struct abbrev_tag *t;
  int size;
  void **slot;

  if (h == NULL)
    {
no_memory:
      error (0, ENOMEM, "%s: Could not read .debug_abbrev", dso->filename);
      if (h)
        htab_delete (h);
      return NULL;
    }

  while ((attr = read_uleb128 (ptr)) != 0)
    {
      size = 10;
      t = malloc (sizeof (*t) + size * sizeof (struct abbrev_attr));
      if (t == NULL)
        goto no_memory;
      t->entry = attr;
      t->nattr = 0;
      slot = htab_find_slot (h, t, INSERT);
      if (slot == NULL)
        {
	  free (t);
	  goto no_memory;
        }
      if (*slot != NULL)
	{
	  error (0, 0, "%s: Duplicate DWARF abbreviation %d", dso->filename,
		 t->entry);
	  free (t);
	  htab_delete (h);
	  return NULL;
	}
      t->tag = read_uleb128 (ptr);
      ++ptr; /* skip children flag.  */
      while ((attr = read_uleb128 (ptr)) != 0)
        {
	  if (t->nattr == size)
	    {
	      size += 10;
	      t = realloc (t, sizeof (*t) + size * sizeof (struct abbrev_attr));
	      if (t == NULL)
		goto no_memory;
	    }
	  form = read_uleb128 (ptr);
	  if (form == 2
	      || (form > DW_FORM_flag_present && form != DW_FORM_ref_sig8))
	    {
	      error (0, 0, "%s: Unknown DWARF DW_FORM_%d", dso->filename, form);
	      htab_delete (h);
	      return NULL;
	    }

	  t->attr[t->nattr].attr = attr;
	  t->attr[t->nattr++].form = form;
        }
      if (read_uleb128 (ptr) != 0)
        {
	  error (0, 0, "%s: DWARF abbreviation does not end with 2 zeros",
		 dso->filename);
	  htab_delete (h);
	  return NULL;
        }
      *slot = t;
    }

  return h;
}

#define IS_DIR_SEPARATOR(c) ((c)=='/')

static char *
canonicalize_path (const char *s, char *d)
{
  char *rv = d;
  char *droot;

  if (IS_DIR_SEPARATOR (*s))
    {
      *d++ = *s++;
      if (IS_DIR_SEPARATOR (*s) && !IS_DIR_SEPARATOR (s[1]))
	{
	  /* Special case for "//foo" meaning a Posix namespace
	     escape.  */
	  *d++ = *s++;
	}
      while (IS_DIR_SEPARATOR (*s))
	s++;
    }
  droot = d;

  while (*s)
    {
      /* At this point, we're always at the beginning of a path
	 segment.  */

      if (s[0] == '.' && (s[1] == 0 || IS_DIR_SEPARATOR (s[1])))
	{
	  s++;
	  if (*s)
	    while (IS_DIR_SEPARATOR (*s))
	      ++s;
	}

      else if (s[0] == '.' && s[1] == '.'
	       && (s[2] == 0 || IS_DIR_SEPARATOR (s[2])))
	{
	  char *pre = d - 1; /* includes slash */
	  while (droot < pre && IS_DIR_SEPARATOR (*pre))
	    pre--;
	  if (droot <= pre && ! IS_DIR_SEPARATOR (*pre))
	    {
	      while (droot < pre && ! IS_DIR_SEPARATOR (*pre))
		pre--;
	      /* pre now points to the slash */
	      if (droot < pre)
		pre++;
	      if (pre + 3 == d && pre[0] == '.' && pre[1] == '.')
		{
		  *d++ = *s++;
		  *d++ = *s++;
		}
	      else
		{
		  d = pre;
		  s += 2;
		  if (*s)
		    while (IS_DIR_SEPARATOR (*s))
		      s++;
		}
	    }
	  else
	    {
	      *d++ = *s++;
	      *d++ = *s++;
	    }
	}
      else
	{
	  while (*s && ! IS_DIR_SEPARATOR (*s))
	    *d++ = *s++;
	}

      if (IS_DIR_SEPARATOR (*s))
	{
	  *d++ = *s++;
	  while (IS_DIR_SEPARATOR (*s))
	    s++;
	}
    }
  while (droot < d && IS_DIR_SEPARATOR (d[-1]))
    --d;
  if (d == rv)
    *d++ = '.';
  *d = 0;

  return rv;
}

/* Returns the rest of PATH if it starts with DIR_PREFIX, skipping any
   / path separators, or NULL if PATH doesn't start with
   DIR_PREFIX. Might return the empty string if PATH equals DIR_PREFIX
   (modulo trailing slashes). Never returns path starting with '/'.
   Note that DIR_PREFIX itself should NOT end with a '/'.  */
static const char *
skip_dir_prefix (const char *path, const char *dir_prefix)
{
  size_t prefix_len = strlen (dir_prefix);
  if (strncmp (path, dir_prefix, prefix_len) == 0)
    {
      path += prefix_len;
      /* Unless path == dir_prefix there should be at least one '/'
	 in the path (which we will skip).  Otherwise the path has
	 a different (longer) directory prefix.  */
      if (*path != '\0' && !IS_DIR_SEPARATOR (*path))
	return NULL;
      while (IS_DIR_SEPARATOR (path[0]))
	path++;
      return path;
    }

  return NULL;
}

/* Most strings will be in the existing debug string table. But to
   replace the base/dest directory prefix we need some new storage.
   Keep new strings somewhat close together for faster comparison and
   copying.  SIZE should be at least one (and includes space for the
   zero terminator). The returned pointer points to uninitialized
   data.  */
static char *
new_string_storage (struct strings *strings, size_t size)
{
  assert (size > 0);

  /* If the string is extra long just create a whole block for
     it. Normally strings are much smaller than STRMEMSIZE. */
  if (strings->last_block == NULL
      || size > STRMEMSIZE
      || strings->stridx > STRMEMSIZE
      || (STRMEMSIZE - strings->stridx) < size)
    {
      struct strmemblock *newblock = malloc (sizeof (struct strmemblock)
					     + MAX (STRMEMSIZE, size));
      if (newblock == NULL)
	return NULL;

      newblock->next = NULL;

      if (strings->blocks == NULL)
	strings->blocks = newblock;

      if (strings->last_block != NULL)
	strings->last_block->next = newblock;

      strings->last_block = newblock;
      strings->stridx = 0;
    }

  size_t stridx = strings->stridx;
  strings->stridx += size + 1;
  return &strings->last_block->memory[stridx];
}

/* Comparison function used for tsearch. */
static int
strent_compare (const void *a, const void *b)
{
  struct stridxentry *entry_a = (struct stridxentry *)a;
  struct stridxentry *entry_b = (struct stridxentry *)b;
  size_t idx_a = entry_a->idx;
  size_t idx_b = entry_b->idx;

  if (idx_a < idx_b)
    return -1;

  if (idx_a > idx_b)
    return 1;

  return 0;
}

/* Allocates and inserts a new entry for the old index if not yet
   seen.  Returns a stridxentry if the given index has not yet been
   seen and needs to be filled in with the associated string (either
   the original string or the replacement string). Returns NULL if the
   idx is already known. Use in phase 0 to add all strings seen. In
   phase 1 use string_find_entry instead to get existing entries. */
static struct stridxentry *
string_find_new_entry (struct strings *strings, size_t old_idx)
{
  /* Use next entry in the pool for lookup so we can use it directly
     if this is a new index. */
  struct stridxentry *entry;

  /* Keep entries close together to make key comparison fast. */
  if (strings->last_entries == NULL || strings->entryidx >= STRIDXENTRIES)
    {
      size_t entriessz = (sizeof (struct strentblock)
			  + (STRIDXENTRIES * sizeof (struct stridxentry)));
      struct strentblock *newentries = malloc (entriessz);
      if (newentries == NULL)
	error (1, errno, "Couldn't allocate new string entries block");
      else
	{
	  if (strings->entries == NULL)
	    strings->entries = newentries;

	  if (strings->last_entries != NULL)
	    strings->last_entries->next = newentries;

	  strings->last_entries = newentries;
	  strings->last_entries->next = NULL;
	  strings->entryidx = 0;
	}
    }

  entry = &strings->last_entries->entry[strings->entryidx];
  entry->idx = old_idx;
  struct stridxentry **tres = tsearch (entry, &strings->strent_root,
				       strent_compare);
  if (tres == NULL)
    error (1, ENOMEM, "Couldn't insert new strtab idx");
  else if (*tres == entry)
    {
      /* idx not yet seen, must add actual str.  */
      strings->entryidx++;
      return entry;
    }

  return NULL; /* We already know about this idx, entry already complete. */
}

static struct stridxentry *
string_find_entry (struct strings *strings, size_t old_idx)
{
  struct stridxentry **ret;
  struct stridxentry key;
  key.idx = old_idx;
  ret = tfind (&key, &strings->strent_root, strent_compare);
  assert (ret != NULL); /* Can only happen for a bad/non-existing old_idx. */
  return *ret;
}

/* Adds a string_idx_entry given an index into the old/existing string
   table. Should be used in phase 0. Does nothing if the index was
   already registered. Otherwise it checks the string associated with
   the index. If the old string doesn't start with base_dir an entry
   will be recorded for the index with the same string. Otherwise a
   string will be recorded where the base_dir prefix will be replaced
   by dest_dir. Returns true if this is a not yet seen index and there
   a replacement file string has been recorded for it, otherwise
   returns false.  */
static bool
record_file_string_entry_idx (struct strings *strings, size_t old_idx)
{
  bool ret = false;
  struct stridxentry *entry = string_find_new_entry (strings, old_idx);
  if (entry != NULL)
    {
      if (old_idx >= debug_sections[DEBUG_STR].size)
	error (1, 0, "Bad string pointer index %zd", old_idx);

      Strent *strent;
      const char *old_str = (char *)debug_sections[DEBUG_STR].data + old_idx;
      const char *file = skip_dir_prefix (old_str, base_dir);
      if (file == NULL)
	{
	  /* Just record the existing string.  */
	  strent = strtab_add_len (strings->str_tab, old_str,
				   strlen (old_str) + 1);
	}
      else
	{
	  /* Create and record the altered file path. */
	  size_t dest_len = strlen (dest_dir);
	  size_t file_len = strlen (file);
	  size_t nsize = dest_len + 1; /* + '\0' */
	  if (file_len > 0)
	    nsize += 1 + file_len;     /* + '/' */
	  char *nname = new_string_storage (strings, nsize);
	  if (nname == NULL)
	    error (1, ENOMEM, "Couldn't allocate new string storage");
	  memcpy (nname, dest_dir, dest_len);
	  if (file_len > 0)
	    {
	      nname[dest_len] = '/';
	      memcpy (nname + dest_len + 1, file, file_len + 1);
	    }
	  else
	    nname[dest_len] = '\0';

	  strent = strtab_add_len (strings->str_tab, nname, nsize);
	  ret = true;
	}
      if (strent == NULL)
	error (1, ENOMEM, "Could not create new string table entry");
      else
	entry->entry = strent;
    }

  return ret;
}

/* Same as record_new_string_file_string_entry_idx but doesn't replace
   base_dir with dest_dir, just records the existing string associated
   with the index. */
static void
record_existing_string_entry_idx (struct strings *strings, size_t old_idx)
{
  struct stridxentry *entry = string_find_new_entry (strings, old_idx);
  if (entry != NULL)
    {
      if (old_idx >= debug_sections[DEBUG_STR].size)
	error (1, 0, "Bad string pointer index %zd", old_idx);

      const char *str = (char *)debug_sections[DEBUG_STR].data + old_idx;
      Strent *strent = strtab_add_len (strings->str_tab,
				       str, strlen (str) + 1);
      if (strent == NULL)
	error (1, ENOMEM, "Could not create new string table entry");
      else
	entry->entry = strent;
    }
}

static void
setup_strings (struct strings *strings)
{
  strings->str_tab = strtab_init (false);
  strings->str_buf = NULL;
  strings->blocks = NULL;
  strings->last_block = NULL;
  strings->entries = NULL;
  strings->last_entries = NULL;
  strings->strent_root = NULL;
}

/* Noop for tdestroy. */
static void free_node (void *p __attribute__((__unused__))) { }

static void
destroy_strings (struct strings *strings)
{
  struct strmemblock *smb = strings->blocks;
  while (smb != NULL)
    {
      void *old = smb;
      smb = smb->next;
      free (old);
    }

  struct strentblock *emb = strings->entries;
  while (emb != NULL)
    {
      void *old = emb;
      emb = emb->next;
      free (old);
    }

  strtab_free (strings->str_tab);
  tdestroy (strings->strent_root, &free_node);
  free (strings->str_buf);
}

/* The minimum number of line tables we pre-allocate. */
#define MIN_LINE_TABLES 64

/* Gets a line_table at offset. Returns true if not yet know and
   successfully read, false otherwise.  Sets *table to NULL and
   outputs a warning if there was a problem reading the table at the
   given offset.  */
static bool
get_line_table (DSO *dso, size_t off, struct line_table **table)
{
  struct debug_lines *lines = &dso->lines;
  /* Assume there aren't that many, just do a linear search.  The
     array is probably already sorted because the stmt_lists are
     probably inserted in order. But we cannot rely on that (maybe we
     should check that to make searching quicker if possible?).  Once
     we have all line tables for phase 1 (rewriting) we do explicitly
     sort the array.*/
  for (int i = 0; i < lines->used; i++)
    if (lines->table[i].old_idx == off)
      {
	*table = &lines->table[i];
	return false;
      }

  if (lines->size == lines->used)
    {
      struct line_table *new_table = realloc (lines->table,
					      (sizeof (struct line_table)
					       * (lines->size
						  + MIN_LINE_TABLES)));
      if (new_table == NULL)
	{
	  error (0, ENOMEM, "Couldn't add more debug_line tables");
	  *table = NULL;
	  return false;
	}
      lines->table = new_table;
      lines->size += MIN_LINE_TABLES;
    }

  struct line_table *t = &lines->table[lines->used];
  *table = NULL;

  t->old_idx = off;
  t->new_idx = off;
  t->size_diff = 0;
  t->replace_dirs = false;
  t->replace_files = false;

  unsigned char *ptr = debug_sections[DEBUG_LINE].data;
  unsigned char *endsec = ptr + debug_sections[DEBUG_LINE].size;
  if (ptr == NULL)
    {
      error (0, 0, "%s: No .line_table section", dso->filename);
      return false;
    }

  if (off > debug_sections[DEBUG_LINE].size)
    {
      error (0, 0, "%s: Invalid .line_table offset 0x%zx",
	     dso->filename, off);
      return false;
    }
  ptr += off;

  /* unit_length */
  unsigned char *endcu = ptr + 4;
  t->unit_length = read_32 (ptr);
  endcu += t->unit_length;
  if (endcu == ptr + 0xffffffff)
    {
      error (0, 0, "%s: 64-bit DWARF not supported", dso->filename);
      return false;
    }

  if (endcu > endsec)
    {
      error (0, 0, "%s: .debug_line CU does not fit into section",
	     dso->filename);
      return false;
    }

  /* version */
  t->version = read_16 (ptr);
  if (t->version != 2 && t->version != 3 && t->version != 4)
    {
      error (0, 0, "%s: DWARF version %d unhandled", dso->filename,
	     t->version);
      return false;
    }

  /* header_length */
  unsigned char *endprol = ptr + 4;
  t->header_length = read_32 (ptr);
  endprol += t->header_length;
  if (endprol > endcu)
    {
      error (0, 0, "%s: .debug_line CU prologue does not fit into CU",
	     dso->filename);
      return false;
    }

  /* min instr len */
  t->min_instr_len = *ptr++;

  /* max op per instr, if version >= 4 */
  if (t->version >= 4)
    t->max_op_per_instr = *ptr++;

  /* default is stmt */
  t->default_is_stmt = *ptr++;

  /* line base */
  t->line_base = (*(int8_t *)ptr++);

  /* line range */
  t->line_range = *ptr++;

  /* opcode base */
  t->opcode_base = *ptr++;

  if (ptr + t->opcode_base - 1 >= endcu)
    {
      error (0, 0, "%s: .debug_line opcode table does not fit into CU",
	     dso->filename);
      return false;
    }
  lines->used++;
  *table = t;
  return true;
}

static int dirty_elf;
static void
dirty_section (unsigned int sec)
{
  elf_flagdata (debug_sections[sec].elf_data, ELF_C_SET, ELF_F_DIRTY);
  dirty_elf = 1;
}

static int
line_table_cmp (const void *a, const void *b)
{
  struct line_table *ta = (struct line_table *) a;
  struct line_table *tb = (struct line_table *) b;

  if (ta->old_idx < tb->old_idx)
    return -1;

  if (ta->old_idx > tb->old_idx)
    return 1;

  return 0;
}


/* Called after phase zero (which records all adjustments needed for
   the line tables referenced from debug_info) and before phase one
   starts (phase one will adjust the .debug_line section stmt
   references using the updated data structures). */
static void
edit_dwarf2_line (DSO *dso)
{
  Elf_Data *linedata = debug_sections[DEBUG_LINE].elf_data;
  int linendx = debug_sections[DEBUG_LINE].sec;
  Elf_Scn *linescn = dso->scn[linendx];
  unsigned char *old_buf = linedata->d_buf;

  /* Out with the old. */
  linedata->d_size = 0;

  /* In with the new. */
  linedata = elf_newdata (linescn);

  dso->lines.line_buf = malloc (dso->lines.debug_lines_len);
  if (dso->lines.line_buf == NULL)
    error (1, ENOMEM, "No memory for new .debug_line table (0x%zx bytes)",
	   dso->lines.debug_lines_len);

  linedata->d_size = dso->lines.debug_lines_len;
  linedata->d_buf = dso->lines.line_buf;
  debug_sections[DEBUG_LINE].size = linedata->d_size;

  /* Make sure the line tables are sorted on the old index. */
  qsort (dso->lines.table, dso->lines.used, sizeof (struct line_table),
	 line_table_cmp);

  unsigned char *ptr = linedata->d_buf;
  for (int ldx = 0; ldx < dso->lines.used; ldx++)
    {
      struct line_table *t = &dso->lines.table[ldx];
      unsigned char *optr = old_buf + t->old_idx;
      t->new_idx = ptr - (unsigned char *) linedata->d_buf;

      /* Just copy the whole table if nothing needs replacing. */
      if (! t->replace_dirs && ! t->replace_files)
	{
	  assert (t->size_diff == 0);
	  memcpy (ptr, optr, t->unit_length + 4);
	  ptr += t->unit_length + 4;
	  continue;
	}

      /* Header fields. */
      write_32 (ptr, t->unit_length + t->size_diff);
      write_16 (ptr, t->version);
      write_32 (ptr, t->header_length + t->size_diff);
      write_8 (ptr, t->min_instr_len);
      if (t->version >= 4)
	write_8 (ptr, t->max_op_per_instr);
      write_8 (ptr, t->default_is_stmt);
      write_8 (ptr, t->line_base);
      write_8 (ptr, t->line_range);
      write_8 (ptr, t->opcode_base);

      optr += (4 /* unit len */
	       + 2 /* version */
	       + 4 /* header len */
	       + 1 /* min instr len */
	       + (t->version >= 4) /* max op per instr, if version >= 4 */
	       + 1 /* default is stmt */
	       + 1 /* line base */
	       + 1 /* line range */
	       + 1); /* opcode base */

      /* opcode len table. */
      memcpy (ptr, optr, t->opcode_base - 1);
      optr += t->opcode_base - 1;
      ptr += t->opcode_base - 1;

      /* directory table. We need to find the end (start of file
	 table) anyway, so loop over all dirs, even if replace_dirs is
	 false. */
      while (*optr != 0)
	{
	  const char *dir = (const char *) optr;
	  const char *file_path = NULL;
	  if (t->replace_dirs)
	    {
	      file_path = skip_dir_prefix (dir, base_dir);
	      if (file_path != NULL)
		{
		  size_t dest_len = strlen (dest_dir);
		  size_t file_len = strlen (file_path);
		  memcpy (ptr, dest_dir, dest_len);
		  ptr += dest_len;
		  if (file_len > 0)
		    {
		      *ptr++ = '/';
		      memcpy (ptr, file_path, file_len);
		      ptr += file_len;
		    }
		  *ptr++ = '\0';
		}
	    }
	  if (file_path == NULL)
	    {
	      size_t dir_len = strlen (dir);
	      memcpy (ptr, dir, dir_len + 1);
	      ptr += dir_len + 1;
	    }

	  optr = (unsigned char *) strchr (dir, 0) + 1;
	}
      optr++;
      *ptr++ = '\0';

      /* file table */
      if (t->replace_files)
	{
	  while (*optr != 0)
	    {
	      const char *file = (const char *) optr;
	      const char *file_path = NULL;
	      if (t->replace_files)
		{
		  file_path = skip_dir_prefix (file, base_dir);
		  if (file_path != NULL)
		    {
		      size_t dest_len = strlen (dest_dir);
		      size_t file_len = strlen (file_path);
		      memcpy (ptr, dest_dir, dest_len);
		      ptr += dest_len;
		      if (file_len > 0)
			{
			  *ptr++ = '/';
			  memcpy (ptr, file_path, file_len);
			  ptr += file_len;
			}
		      *ptr++ = '\0';
		    }
		}
	      if (file_path == NULL)
		{
		  size_t file_len = strlen (file);
		  memcpy (ptr, file, file_len + 1);
		  ptr += file_len + 1;
		}

	      optr = (unsigned char *) strchr (file, 0) + 1;

	      /* dir idx, time, len */
	      uint32_t dir_idx = read_uleb128 (optr);
	      write_uleb128 (ptr, dir_idx);
	      uint32_t time = read_uleb128 (optr);
	      write_uleb128 (ptr, time);
	      uint32_t len = read_uleb128 (optr);
	      write_uleb128 (ptr, len);
	    }
	  optr++;
	  *ptr++ = '\0';
	}

      /* line number program (and file table if not copied above). */
      size_t remaining = (t->unit_length + 4
			  - (optr - (old_buf + t->old_idx)));
      memcpy (ptr, optr, remaining);
      ptr += remaining;
    }
}

/* Called during phase zero for each debug_line table referenced from
   .debug_info.  Outputs all source files seen and records any
   adjustments needed in the debug_list data structures. Returns true
   if line_table needs to be rewrite either the dir or file paths. */
static bool
read_dwarf2_line (DSO *dso, uint32_t off, char *comp_dir)
{
  unsigned char *ptr, *dir;
  unsigned char **dirt;
  uint32_t value, dirt_cnt;
  size_t comp_dir_len = !comp_dir ? 0 : strlen (comp_dir);
  struct line_table *table;

  if (get_line_table (dso, off, &table) == false
      || table == NULL)
    {
      if (table != NULL)
	error (0, 0, ".debug_line offset 0x%x referenced multiple times",
	       off);
      return false;
    }

  /* Skip to the directory table. The rest of the header has already
     been read and checked by get_line_table. */
  ptr = debug_sections[DEBUG_LINE].data + off;
  ptr += (4 /* unit len */
	  + 2 /* version */
	  + 4 /* header len */
	  + 1 /* min instr len */
	  + (table->version >= 4) /* max op per instr, if version >= 4 */
	  + 1 /* default is stmt */
	  + 1 /* line base */
	  + 1 /* line range */
	  + 1 /* opcode base */
	  + table->opcode_base - 1); /* opcode len table */
  dir = ptr;

  /* dir table: */
  value = 1;
  while (*ptr != 0)
    {
      if (base_dir && dest_dir)
	{
	  /* Do we need to replace any of the dirs? Calculate new size. */
	  const char *file_path = skip_dir_prefix ((const char *)ptr,
						   base_dir);
	  if (file_path != NULL)
	    {
	      size_t old_size = strlen ((const char *)ptr) + 1;
	      size_t file_len = strlen (file_path);
	      size_t new_size = strlen (dest_dir) + 1;
	      if (file_len > 0)
		new_size += 1 + file_len;
	      table->size_diff += (new_size - old_size);
	      table->replace_dirs = true;
	    }
	}

      ptr = (unsigned char *) strchr ((char *)ptr, 0) + 1;
      ++value;
    }

  dirt = (unsigned char **) alloca (value * sizeof (unsigned char *));
  dirt[0] = (unsigned char *) ".";
  dirt_cnt = 1;
  ptr = dir;
  while (*ptr != 0)
    {
      dirt[dirt_cnt++] = ptr;
      ptr = (unsigned char *) strchr ((char *)ptr, 0) + 1;
    }
  ptr++;

  /* file table: */
  while (*ptr != 0)
    {
      char *s, *file;
      size_t file_len, dir_len;

      file = (char *) ptr;
      ptr = (unsigned char *) strchr ((char *)ptr, 0) + 1;
      value = read_uleb128 (ptr);

      if (value >= dirt_cnt)
	{
	  error (0, 0, "%s: Wrong directory table index %u",
		 dso->filename, value);
	  return false;
	}
      file_len = strlen (file);
      if (base_dir && dest_dir)
	{
	  /* Do we need to replace any of the files? Calculate new size. */
	  const char *file_path = skip_dir_prefix (file, base_dir);
	  if (file_path != NULL)
	    {
	      size_t old_size = file_len + 1;
	      size_t file_len = strlen (file_path);
	      size_t new_size = strlen (dest_dir) + 1;
	      if (file_len > 0)
		new_size += 1 + file_len;
	      table->size_diff += (new_size - old_size);
	      table->replace_files = true;
	    }
	}
      dir_len = strlen ((char *)dirt[value]);
      s = malloc (comp_dir_len + 1 + file_len + 1 + dir_len + 1);
      if (s == NULL)
	{
	  error (0, ENOMEM, "%s: Reading file table", dso->filename);
	  return false;
	}
      if (*file == '/')
	{
	  memcpy (s, file, file_len + 1);
	}
      else if (*dirt[value] == '/')
	{
	  memcpy (s, dirt[value], dir_len);
	  s[dir_len] = '/';
	  memcpy (s + dir_len + 1, file, file_len + 1);
	}
      else
	{
	  char *p = s;
	  if (comp_dir_len != 0)
	    {
	      memcpy (s, comp_dir, comp_dir_len);
	      s[comp_dir_len] = '/';
	      p += comp_dir_len + 1;
	    }
	  memcpy (p, dirt[value], dir_len);
	  p[dir_len] = '/';
	  memcpy (p + dir_len + 1, file, file_len + 1);
	}
      canonicalize_path (s, s);
      if (list_file_fd != -1)
	{
	  const char *p = NULL;
	  if (base_dir == NULL)
	    p = s;
	  else
	    {
	      p = skip_dir_prefix (s, base_dir);
	      if (p == NULL && dest_dir != NULL)
		p = skip_dir_prefix (s, dest_dir);
	    }

	  if (p)
	    {
	      size_t size = strlen (p) + 1;
	      while (size > 0)
		{
		  ssize_t ret = write (list_file_fd, p, size);
		  if (ret == -1)
		    break;
		  size -= ret;
		  p += ret;
		}
	    }
	}

      free (s);

      read_uleb128 (ptr);
      read_uleb128 (ptr);
    }

  dso->lines.debug_lines_len += 4 + table->unit_length + table->size_diff;
  return table->replace_dirs || table->replace_files;
}

/* Called during phase one, after the table has been sorted. */
static size_t
find_new_list_offs (struct debug_lines *lines, size_t idx)
{
  struct line_table key;
  key.old_idx = idx;
  struct line_table *table = bsearch (&key, lines->table,
				      lines->used,
				      sizeof (struct line_table),
				      line_table_cmp);
  return table->new_idx;
}

/* This scans the attributes of one DIE described by the given abbrev_tag.
   PTR points to the data in the debug_info. It will be advanced till all
   abbrev data is consumed. In phase zero data is collected, in phase one
   data might be replaced/updated.  */
static unsigned char *
edit_attributes (DSO *dso, unsigned char *ptr, struct abbrev_tag *t, int phase)
{
  int i;
  uint32_t list_offs;
  int found_list_offs;
  char *comp_dir;

  comp_dir = NULL;
  list_offs = 0;
  found_list_offs = 0;
  for (i = 0; i < t->nattr; ++i)
    {
      uint32_t form = t->attr[i].form;
      size_t len = 0;
      while (1)
	{
	  /* Whether we already handled a string as file for this
	     attribute.  If we did then we don't need to handle/record
	     it again when handling the DW_FORM_strp later. */
	  bool handled_strp = false;

	  /* A stmt_list points into the .debug_line section.  In
	     phase zero record all offsets. Then in phase one replace
	     them with the new offsets if we rewrote the line
	     tables.  */
	  if (t->attr[i].attr == DW_AT_stmt_list)
	    {
	      if (form == DW_FORM_data4
		  || form == DW_FORM_sec_offset)
		{
		  list_offs = do_read_32_relocated (ptr);
		  if (phase == 0)
		    found_list_offs = 1;
		  else if (need_stmt_update) /* phase one */
		    {
		      size_t idx, new_idx;
		      idx = do_read_32_relocated (ptr);
		      new_idx = find_new_list_offs (&dso->lines, idx);
		      do_write_32_relocated (ptr, new_idx);
		    }
		}
	    }

	  /* DW_AT_comp_dir is the current working directory. */
	  if (t->attr[i].attr == DW_AT_comp_dir)
	    {
	      if (form == DW_FORM_string)
		{
		  free (comp_dir);
		  comp_dir = strdup ((char *)ptr);

		  if (dest_dir)
		    {
		      /* In phase zero we are just collecting dir/file
			 names and check whether any need to be
			 adjusted. If so, in phase one we replace
			 those dir/files.  */
		      const char *file = skip_dir_prefix (comp_dir, base_dir);
		      if (file != NULL && phase == 0)
			need_string_replacement = true;
		      else if (file != NULL && phase == 1)
			{
			  size_t orig_len = strlen (comp_dir);
			  size_t dest_len = strlen (dest_dir);
			  size_t file_len = strlen (file);
			  size_t new_len = dest_len;
			  if (file_len > 0)
			    new_len += 1 + file_len; /* + '/' */

			  /* We don't want to rewrite the whole
			     debug_info section, so we only replace
			     the comp_dir with something equal or
			     smaller, possibly adding some slashes
			     at the end of the new compdir.  This
			     normally doesn't happen since most
			     producers will use DW_FORM_strp which is
			     more efficient.  */
			  if (orig_len < new_len)
			    fprintf (stderr, "Warning, not replacing comp_dir "
				     "'%s' prefix ('%s' -> '%s') encoded as "
				     "DW_FORM_string. "
				     "Replacement too large.\n",
				     comp_dir, base_dir, dest_dir);
			  else
			    {
			      /* Add zero (if no file part), one or more
				 slashes in between the new dest_dir and the
				 file name to fill up all space (replacement
				 DW_FORM_string must be of the same length).
				 We don't need to copy the old file name (if
				 any) or the zero terminator, because those
				 are already at the end of the string.  */
			      memcpy (ptr, dest_dir, dest_len);
			      memset (ptr + dest_len, '/',
				      orig_len - new_len);
			    }
			}
		    }
		}
	      else if (form == DW_FORM_strp &&
		       debug_sections[DEBUG_STR].data)
		{
		  const char *dir;
		  size_t idx = do_read_32_relocated (ptr);
		  /* In phase zero we collect the comp_dir.  */
		  if (phase == 0)
		    {
		      if (idx >= debug_sections[DEBUG_STR].size)
			error (1, 0,
			       "%s: Bad string pointer index %zd for comp_dir",
			       dso->filename, idx);
		      dir = (char *) debug_sections[DEBUG_STR].data + idx;

		      free (comp_dir);
		      comp_dir = strdup (dir);
		    }

		  if (dest_dir != NULL && phase == 0)
		    {
		      if (record_file_string_entry_idx (&dso->strings, idx))
			need_strp_update = true;
		      handled_strp = true;
		    }
		}
	    }
	  else if ((t->tag == DW_TAG_compile_unit
		    || t->tag == DW_TAG_partial_unit)
		   && t->attr[i].attr == DW_AT_name
		   && form == DW_FORM_strp
		   && debug_sections[DEBUG_STR].data)
	    {
	      /* DW_AT_name is the primary file for this compile
		 unit. If starting with / it is a full path name.
		 Note that we don't handle DW_FORM_string in this
		 case.  */
	      size_t idx = do_read_32_relocated (ptr);

	      /* In phase zero we will look for a comp_dir to use.  */
	      if (phase == 0)
		{
		  if (idx >= debug_sections[DEBUG_STR].size)
		    error (1, 0,
			   "%s: Bad string pointer index %zd for unit name",
			   dso->filename, idx);
		  char *name = (char *) debug_sections[DEBUG_STR].data + idx;
		  if (*name == '/' && comp_dir == NULL)
		    {
		      char *enddir = strrchr (name, '/');

		      if (enddir != name)
			{
			  comp_dir = malloc (enddir - name + 1);
			  memcpy (comp_dir, name, enddir - name);
			  comp_dir [enddir - name] = '\0';
			}
		      else
			comp_dir = strdup ("/");
		    }
		}

	      /* First pass (0) records the new name to be
		 added to the debug string pool, the second
		 pass (1) stores it (the new index). */
	      if (dest_dir && phase == 0)
		{
		  if (record_file_string_entry_idx (&dso->strings, idx))
		    need_strp_update = true;
		  handled_strp = true;
		}
	    }

	  switch (form)
	    {
	    case DW_FORM_ref_addr:
	      if (cu_version == 2)
		ptr += ptr_size;
	      else
		ptr += 4;
	      break;
	    case DW_FORM_flag_present:
	      break;
	    case DW_FORM_addr:
	      ptr += ptr_size;
	      break;
	    case DW_FORM_ref1:
	    case DW_FORM_flag:
	    case DW_FORM_data1:
	      ++ptr;
	      break;
	    case DW_FORM_ref2:
	    case DW_FORM_data2:
	      ptr += 2;
	      break;
	    case DW_FORM_ref4:
	    case DW_FORM_data4:
	    case DW_FORM_sec_offset:
	      ptr += 4;
	      break;
	    case DW_FORM_ref8:
	    case DW_FORM_data8:
	    case DW_FORM_ref_sig8:
	      ptr += 8;
	      break;
	    case DW_FORM_sdata:
	    case DW_FORM_ref_udata:
	    case DW_FORM_udata:
	      read_uleb128 (ptr);
	      break;
	    case DW_FORM_strp:
	      /* In the first pass we collect all strings, in the
		 second we put the new references back (if there are
		 any changes).  */
	      if (phase == 0)
		{
		  /* handled_strp is set for attributes referring to
		     files. If it is set the string is already
		     recorded. */
		  if (! handled_strp)
		    {
		      size_t idx = do_read_32_relocated (ptr);
		      record_existing_string_entry_idx (&dso->strings, idx);
		    }
		}
	      else if (need_strp_update) /* && phase == 1 */
		{
		  struct stridxentry *entry;
		  size_t idx, new_idx;
		  idx = do_read_32_relocated (ptr);
		  entry = string_find_entry (&dso->strings, idx);
		  new_idx = strent_offset (entry->entry);
		  do_write_32_relocated (ptr, new_idx);
		}
	      ptr += 4;
	      break;
	    case DW_FORM_string:
	      ptr = (unsigned char *) strchr ((char *)ptr, '\0') + 1;
	      break;
	    case DW_FORM_indirect:
	      form = read_uleb128 (ptr);
	      continue;
	    case DW_FORM_block1:
	      len = *ptr++;
	      break;
	    case DW_FORM_block2:
	      len = read_16 (ptr);
	      form = DW_FORM_block1;
	      break;
	    case DW_FORM_block4:
	      len = read_32 (ptr);
	      form = DW_FORM_block1;
	      break;
	    case DW_FORM_block:
	    case DW_FORM_exprloc:
	      len = read_uleb128 (ptr);
	      form = DW_FORM_block1;
	      assert (len < UINT_MAX);
	      break;
	    default:
	      error (0, 0, "%s: Unknown DWARF DW_FORM_%d", dso->filename,
		     form);
	      return NULL;
	    }

	  if (form == DW_FORM_block1)
	    ptr += len;

	  break;
	}
    }

  /* Ensure the CU current directory will exist even if only empty.  Source
     filenames possibly located in its parent directories refer relatively to
     it and the debugger (GDB) cannot safely optimize out the missing
     CU current dir subdirectories.  Only do this once in phase one. And
     only do this for dirs under our build/base_dir.  Don't output the
     empty string (in case the comp_dir == base_dir).  */
  if (phase == 0 && base_dir && comp_dir && list_file_fd != -1)
    {
      const char *p = skip_dir_prefix (comp_dir, base_dir);
      if (p != NULL && p[0] != '\0')
        {
	  size_t size = strlen (p) + 1;
	  while (size > 0)
	    {
	      ssize_t ret = write (list_file_fd, p, size);
	      if (ret == -1)
		break;
	      size -= ret;
	      p += ret;
	    }
	}
    }

  /* In phase zero we collect all file names (we need the comp_dir for
     that).  Note that calculating the new size and offsets is done
     separately (at the end of phase zero after all CUs have been
     scanned in dwarf2_edit). */
  if (phase == 0 && found_list_offs
      && read_dwarf2_line (dso, list_offs, comp_dir))
    need_stmt_update = true;

  free (comp_dir);

  return ptr;
}

static int
line_rel_cmp (const void *a, const void *b)
{
  LINE_REL *rela = (LINE_REL *) a, *relb = (LINE_REL *) b;

  if (rela->r_offset < relb->r_offset)
    return -1;

  if (rela->r_offset > relb->r_offset)
    return 1;

  return 0;
}

static int
edit_dwarf2 (DSO *dso)
{
  Elf_Data *data;
  Elf_Scn *scn;
  int i, j;

  for (i = 0; debug_sections[i].name; ++i)
    {
      debug_sections[i].data = NULL;
      debug_sections[i].size = 0;
      debug_sections[i].sec = 0;
      debug_sections[i].relsec = 0;
    }
  ptr_size = 0;

  for (i = 1; i < dso->ehdr.e_shnum; ++i)
    if (! (dso->shdr[i].sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR))
	&& dso->shdr[i].sh_size)
      {
        const char *name = strptr (dso, dso->ehdr.e_shstrndx,
				   dso->shdr[i].sh_name);

	if (strncmp (name, ".debug_", sizeof (".debug_") - 1) == 0)
	  {
	    for (j = 0; debug_sections[j].name; ++j)
	      if (strcmp (name, debug_sections[j].name) == 0)
	 	{
		  struct debug_section *debug_sec = &debug_sections[j];
		  if (debug_sections[j].data)
		    {
		      if (j != DEBUG_MACRO)
			{
			  error (0, 0, "%s: Found two copies of %s section",
				 dso->filename, name);
			  return 1;
			}
		      else
			{
			  /* In relocatable files .debug_macro might
			     appear multiple times as COMDAT
			     section.  */
			  struct debug_section *sec;
			  sec = calloc (sizeof (struct debug_section), 1);
			  if (sec == NULL)
			    error (1, errno,
				   "%s: Could not allocate more macro sections",
				   dso->filename);
			  sec->name = ".debug_macro";

			  struct debug_section *macro_sec = debug_sec;
			  while (macro_sec->next != NULL)
			    macro_sec = macro_sec->next;

			  macro_sec->next = sec;
			  debug_sec = sec;
			}
		    }

		  scn = dso->scn[i];
		  data = elf_getdata (scn, NULL);
		  assert (data != NULL && data->d_buf != NULL);
		  assert (elf_getdata (scn, data) == NULL);
		  assert (data->d_off == 0);
		  assert (data->d_size == dso->shdr[i].sh_size);
		  debug_sec->data = data->d_buf;
		  debug_sec->elf_data = data;
		  debug_sec->size = data->d_size;
		  debug_sec->sec = i;
		  break;
		}

	    if (debug_sections[j].name == NULL)
	      {
		error (0, 0, "%s: Unknown debugging section %s",
		       dso->filename, name);
	      }
	  }
	else if (dso->ehdr.e_type == ET_REL
		 && ((dso->shdr[i].sh_type == SHT_REL
		      && strncmp (name, ".rel.debug_",
				  sizeof (".rel.debug_") - 1) == 0)
		     || (dso->shdr[i].sh_type == SHT_RELA
			 && strncmp (name, ".rela.debug_",
				     sizeof (".rela.debug_") - 1) == 0)))
	  {
	    for (j = 0; debug_sections[j].name; ++j)
	      if (strcmp (name + sizeof (".rel") - 1
			  + (dso->shdr[i].sh_type == SHT_RELA),
			  debug_sections[j].name) == 0)
	 	{
		  if (j == DEBUG_MACRO)
		    {
		      /* Pick the correct one.  */
		      int rel_target = dso->shdr[i].sh_info;
		      struct debug_section *macro_sec = &debug_sections[j];
		      while (macro_sec != NULL)
			{
			  if (macro_sec->sec == rel_target)
			    {
			      macro_sec->relsec = i;
			      break;
			    }
			  macro_sec = macro_sec->next;
			}
		      if (macro_sec == NULL)
			error (0, 1, "No .debug_macro reloc section: %s",
			       dso->filename);
		    }
		  else
		    debug_sections[j].relsec = i;
		  break;
		}
	  }
      }

  if (dso->ehdr.e_ident[EI_DATA] == ELFDATA2LSB)
    {
      do_read_16 = buf_read_ule16;
      do_read_32 = buf_read_ule32;
      do_write_16 = dwarf2_write_le16;
      do_write_32 = dwarf2_write_le32;
    }
  else if (dso->ehdr.e_ident[EI_DATA] == ELFDATA2MSB)
    {
      do_read_16 = buf_read_ube16;
      do_read_32 = buf_read_ube32;
      do_write_16 = dwarf2_write_be16;
      do_write_32 = dwarf2_write_be32;
    }
  else
    {
      error (0, 0, "%s: Wrong ELF data enconding", dso->filename);
      return 1;
    }

  if (debug_sections[DEBUG_INFO].data != NULL)
    {
      unsigned char *ptr, *endcu, *endsec;
      uint32_t value;
      htab_t abbrev;
      struct abbrev_tag tag, *t;
      int phase;
      bool info_rel_updated = false;
      bool macro_rel_updated = false;

      for (phase = 0; phase < 2; phase++)
	{
	  /* If we don't need to update anyhing, skip phase 1. */
	  if (phase == 1
	      && !need_strp_update
	      && !need_string_replacement
	      && !need_stmt_update)
	    break;

	  ptr = debug_sections[DEBUG_INFO].data;
	  setup_relbuf(dso, &debug_sections[DEBUG_INFO], &reltype);
	  rel_updated = false;
	  endsec = ptr + debug_sections[DEBUG_INFO].size;
	  while (ptr < endsec)
	    {
	      if (ptr + 11 > endsec)
		{
		  error (0, 0, "%s: .debug_info CU header too small",
			 dso->filename);
		  return 1;
		}

	      endcu = ptr + 4;
	      endcu += read_32 (ptr);
	      if (endcu == ptr + 0xffffffff)
		{
		  error (0, 0, "%s: 64-bit DWARF not supported", dso->filename);
		  return 1;
		}

	      if (endcu > endsec)
		{
		  error (0, 0, "%s: .debug_info too small", dso->filename);
		  return 1;
		}

	      cu_version = read_16 (ptr);
	      if (cu_version != 2 && cu_version != 3 && cu_version != 4)
		{
		  error (0, 0, "%s: DWARF version %d unhandled", dso->filename,
			 cu_version);
		  return 1;
		}

	      value = read_32_relocated (ptr);
	      if (value >= debug_sections[DEBUG_ABBREV].size)
		{
		  if (debug_sections[DEBUG_ABBREV].data == NULL)
		    error (0, 0, "%s: .debug_abbrev not present", dso->filename);
		  else
		    error (0, 0, "%s: DWARF CU abbrev offset too large",
			   dso->filename);
		  return 1;
		}

	      if (ptr_size == 0)
		{
		  ptr_size = read_8 (ptr);
		  if (ptr_size != 4 && ptr_size != 8)
		    {
		      error (0, 0, "%s: Invalid DWARF pointer size %d",
			     dso->filename, ptr_size);
		      return 1;
		    }
		}
	      else if (read_8 (ptr) != ptr_size)
		{
		  error (0, 0, "%s: DWARF pointer size differs between CUs",
			 dso->filename);
		  return 1;
		}

	      abbrev = read_abbrev (dso,
				    debug_sections[DEBUG_ABBREV].data + value);
	      if (abbrev == NULL)
		return 1;

	      while (ptr < endcu)
		{
		  tag.entry = read_uleb128 (ptr);
		  if (tag.entry == 0)
		    continue;
		  t = htab_find_with_hash (abbrev, &tag, tag.entry);
		  if (t == NULL)
		    {
		      error (0, 0, "%s: Could not find DWARF abbreviation %d",
			     dso->filename, tag.entry);
		      htab_delete (abbrev);
		      return 1;
		    }

		  ptr = edit_attributes (dso, ptr, t, phase);
		  if (ptr == NULL)
		    break;
		}

	      htab_delete (abbrev);
	    }

	  /* Remember whether any .debug_info relocations might need
	     to be updated. */
	  info_rel_updated = rel_updated;

	  /* We might have to recalculate/rewrite the debug_line
	     section.  We need to do that before going into phase one
	     so we have all new offsets.  We do this separately from
	     scanning the dirs/file names because the DW_AT_stmt_lists
	     might not be in order or skip some padding we might have
	     to (re)move. */
	  if (phase == 0 && need_stmt_update)
	    {
	      edit_dwarf2_line (dso);

	      /* The line table programs will be moved
		 forward/backwards a bit in the new data. Update the
		 debug_line relocations to the new offsets. */
	      int rndx = debug_sections[DEBUG_LINE].relsec;
	      if (rndx != 0)
		{
		  LINE_REL *rbuf;
		  size_t rels;
		  Elf_Data *rdata = elf_getdata (dso->scn[rndx], NULL);
		  int rtype = dso->shdr[rndx].sh_type;
		  rels = dso->shdr[rndx].sh_size / dso->shdr[rndx].sh_entsize;
		  rbuf = malloc (rels * sizeof (LINE_REL));
		  if (rbuf == NULL)
		    error (1, errno, "%s: Could not allocate line relocations",
			   dso->filename);

		  /* Sort them by offset into section. */
		  for (size_t i = 0; i < rels; i++)
		    {
		      if (rtype == SHT_RELA)
			{
			  GElf_Rela rela;
			  if (gelf_getrela (rdata, i, &rela) == NULL)
			    error (1, 0, "Couldn't get relocation: %s",
				   elf_errmsg (-1));
			  rbuf[i].r_offset = rela.r_offset;
			  rbuf[i].ndx = i;
			}
		      else
			{
			  GElf_Rel rel;
			  if (gelf_getrel (rdata, i, &rel) == NULL)
			    error (1, 0, "Couldn't get relocation: %s",
				   elf_errmsg (-1));
			  rbuf[i].r_offset = rel.r_offset;
			  rbuf[i].ndx = i;
			}
		    }
		  qsort (rbuf, rels, sizeof (LINE_REL), line_rel_cmp);

		  size_t lndx = 0;
		  for (size_t i = 0; i < rels; i++)
		    {
		      /* These relocations only happen in ET_REL files
			 and are section offsets. */
		      GElf_Addr r_offset;
		      size_t ndx = rbuf[i].ndx;

		      GElf_Rel rel;
		      GElf_Rela rela;
		      if (rtype == SHT_RELA)
			{
			  if (gelf_getrela (rdata, ndx, &rela) == NULL)
			    error (1, 0, "Couldn't get relocation: %s",
				   elf_errmsg (-1));
			  r_offset = rela.r_offset;
			}
		      else
			{
			  if (gelf_getrel (rdata, ndx, &rel) == NULL)
			    error (1, 0, "Couldn't get relocation: %s",
				   elf_errmsg (-1));
			  r_offset = rel.r_offset;
			}

		      while (lndx < dso->lines.used
			     && r_offset > (dso->lines.table[lndx].old_idx
					    + 4
					    + dso->lines.table[lndx].unit_length))
			lndx++;

		      if (lndx >= dso->lines.used)
			error (1, 0,
			       ".debug_line relocation offset out of range");

		      /* Offset (pointing into the line program) moves
			 from old to new index including the header
			 size diff. */
		      r_offset += (ssize_t)((dso->lines.table[lndx].new_idx
					     - dso->lines.table[lndx].old_idx)
					    + dso->lines.table[lndx].size_diff);

		      if (rtype == SHT_RELA)
			{
			  rela.r_offset = r_offset;
			  if (gelf_update_rela (rdata, ndx, &rela) == 0)
			    error (1, 0, "Couldn't update relocation: %s",
				   elf_errmsg (-1));
			}
		      else
			{
			  rel.r_offset = r_offset;
			  if (gelf_update_rel (rdata, ndx, &rel) == 0)
			    error (1, 0, "Couldn't update relocation: %s",
				   elf_errmsg (-1));
			}
		    }

		  elf_flagdata (rdata, ELF_C_SET, ELF_F_DIRTY);
		  free (rbuf);
		}
	    }

	  /* The .debug_macro section also contains offsets into the
	     .debug_str section and references to the .debug_line
	     tables, so we need to update those as well if we update
	     the strings or the stmts.  */
	  if ((need_strp_update || need_stmt_update)
	      && debug_sections[DEBUG_MACRO].data)
	    {
	      /* There might be multiple (COMDAT) .debug_macro sections.  */
	      struct debug_section *macro_sec = &debug_sections[DEBUG_MACRO];
	      while (macro_sec != NULL)
		{
		  setup_relbuf(dso, macro_sec, &reltype);
		  rel_updated = false;

		  ptr = macro_sec->data;
		  endsec = ptr + macro_sec->size;
		  int op = 0, macro_version, macro_flags;
		  int offset_len = 4, line_offset = 0;

		  while (ptr < endsec)
		    {
		      if (!op)
			{
			  macro_version = read_16 (ptr);
			  macro_flags = read_8 (ptr);
			  if (macro_version < 4 || macro_version > 5)
			    error (1, 0, "unhandled .debug_macro version: %d",
				   macro_version);
			  if ((macro_flags & ~2) != 0)
			    error (1, 0, "unhandled .debug_macro flags: 0x%x",
				   macro_flags);

			  offset_len = (macro_flags & 0x01) ? 8 : 4;
			  line_offset = (macro_flags & 0x02) ? 1 : 0;

			  if (offset_len != 4)
			    error (0, 1,
				   "Cannot handle 8 byte macro offsets: %s",
				   dso->filename);

			  /* Update the line_offset if it is there.  */
			  if (line_offset)
			    {
			      if (phase == 0)
				ptr += offset_len;
			      else
				{
				  size_t idx, new_idx;
				  idx = do_read_32_relocated (ptr);
				  new_idx = find_new_list_offs (&dso->lines,
								idx);
				  write_32_relocated (ptr, new_idx);
				}
			    }
			}

		      op = read_8 (ptr);
		      if (!op)
			continue;
		      switch(op)
			{
			case DW_MACRO_GNU_define:
			case DW_MACRO_GNU_undef:
			  read_uleb128 (ptr);
			  ptr = ((unsigned char *) strchr ((char *) ptr, '\0')
				 + 1);
			  break;
			case DW_MACRO_GNU_start_file:
			  read_uleb128 (ptr);
			  read_uleb128 (ptr);
			  break;
			case DW_MACRO_GNU_end_file:
			  break;
			case DW_MACRO_GNU_define_indirect:
			case DW_MACRO_GNU_undef_indirect:
			  read_uleb128 (ptr);
			  if (phase == 0)
			    {
			      size_t idx = read_32_relocated (ptr);
			      record_existing_string_entry_idx (&dso->strings,
								idx);
			    }
			  else
			    {
			      struct stridxentry *entry;
			      size_t idx, new_idx;
			      idx = do_read_32_relocated (ptr);
			      entry = string_find_entry (&dso->strings, idx);
			      new_idx = strent_offset (entry->entry);
			      write_32_relocated (ptr, new_idx);
			    }
			  break;
			case DW_MACRO_GNU_transparent_include:
			  ptr += offset_len;
			  break;
			default:
			  error (1, 0, "Unhandled DW_MACRO op 0x%x", op);
			  break;
			}
		    }

		  if (rel_updated)
		    macro_rel_updated = true;
		  macro_sec = macro_sec->next;
		}
	    }

	  /* Same for the debug_str section. Make sure everything is
	     in place for phase 1 updating of debug_info
	     references. */
	  if (phase == 0 && need_strp_update)
	    {
	      Strtab *strtab = dso->strings.str_tab;
	      Elf_Data *strdata = debug_sections[DEBUG_STR].elf_data;
	      int strndx = debug_sections[DEBUG_STR].sec;
	      Elf_Scn *strscn = dso->scn[strndx];

	      /* Out with the old. */
	      strdata->d_size = 0;
	      /* In with the new. */
	      strdata = elf_newdata (strscn);

	      /* We really should check whether we had enough memory,
		 but the old ebl version will just abort on out of
		 memory... */
	      strtab_finalize (strtab, strdata);
	      debug_sections[DEBUG_STR].size = strdata->d_size;
	      dso->strings.str_buf = strdata->d_buf;
	    }

	}

      /* After phase 1 we might have rewritten the debug_info with
	 new strp, strings and/or linep offsets.  */
      if (need_strp_update || need_string_replacement || need_stmt_update)
	dirty_section (DEBUG_INFO);
      if (need_strp_update || need_stmt_update)
	dirty_section (DEBUG_MACRO);
      if (need_stmt_update)
	dirty_section (DEBUG_LINE);

      /* Update any relocations addends we might have touched. */
      if (info_rel_updated)
	update_rela_data (dso, &debug_sections[DEBUG_INFO]);

      if (macro_rel_updated)
	{
	  struct debug_section *macro_sec = &debug_sections[DEBUG_MACRO];
	  while (macro_sec != NULL)
	    {
	      update_rela_data (dso, macro_sec);
	      macro_sec = macro_sec->next;
	    }
	}
    }

  return 0;
}

static struct poptOption optionsTable[] = {
    { "base-dir",  'b', POPT_ARG_STRING, &base_dir, 0,
      "base build directory of objects", NULL },
    { "dest-dir",  'd', POPT_ARG_STRING, &dest_dir, 0,
      "directory to rewrite base-dir into", NULL },
    { "list-file",  'l', POPT_ARG_STRING, &list_file, 0,
      "file where to put list of source and header file names", NULL },
    { "build-id",  'i', POPT_ARG_NONE, &do_build_id, 0,
      "recompute build ID note and print ID on stdout", NULL },
    { "build-id-seed", 's', POPT_ARG_STRING, &build_id_seed, 0,
      "if recomputing the build ID note use this string as hash seed", NULL },
    { "no-recompute-build-id",  'n', POPT_ARG_NONE, &no_recompute_build_id, 0,
      "do not recompute build ID note even when -i or -s are given", NULL },
    { "version", '\0', POPT_ARG_NONE, &show_version, 0,
      "print the debugedit version", NULL },
      POPT_AUTOHELP
    { NULL, 0, 0, NULL, 0, NULL, NULL }
};

static DSO *
fdopen_dso (int fd, const char *name)
{
  Elf *elf = NULL;
  GElf_Ehdr ehdr;
  int i;
  DSO *dso = NULL;
  size_t phnum;

  elf = elf_begin (fd, ELF_C_RDWR, NULL);
  if (elf == NULL)
    {
      error (0, 0, "cannot open ELF file: %s", elf_errmsg (-1));
      goto error_out;
    }

  if (elf_kind (elf) != ELF_K_ELF)
    {
      error (0, 0, "\"%s\" is not an ELF file", name);
      goto error_out;
    }

  if (gelf_getehdr (elf, &ehdr) == NULL)
    {
      error (0, 0, "cannot get the ELF header: %s",
	     elf_errmsg (-1));
      goto error_out;
    }

  if (ehdr.e_type != ET_DYN && ehdr.e_type != ET_EXEC && ehdr.e_type != ET_REL)
    {
      error (0, 0, "\"%s\" is not a shared library", name);
      goto error_out;
    }

  /* Allocate DSO structure. Leave place for additional 20 new section
     headers.  */
  dso = (DSO *)
	malloc (sizeof(DSO) + (ehdr.e_shnum + 20) * sizeof(GElf_Shdr)
	        + (ehdr.e_shnum + 20) * sizeof(Elf_Scn *));
  if (!dso)
    {
      error (0, ENOMEM, "Could not open DSO");
      goto error_out;
    }

  if (elf_getphdrnum (elf, &phnum) != 0)
    {
      error (0, 0, "Couldn't get number of phdrs: %s", elf_errmsg (-1));
      goto error_out;
    }

  /* If there are phdrs we want to maintain the layout of the
     allocated sections in the file.  */
  if (phnum != 0)
    elf_flagelf (elf, ELF_C_SET, ELF_F_LAYOUT);

  memset (dso, 0, sizeof(DSO));
  dso->elf = elf;
  dso->phnum = phnum;
  dso->ehdr = ehdr;
  dso->scn = (Elf_Scn **) &dso->shdr[ehdr.e_shnum + 20];

  for (i = 0; i < ehdr.e_shnum; ++i)
    {
      dso->scn[i] = elf_getscn (elf, i);
      gelf_getshdr (dso->scn[i], dso->shdr + i);
    }

  dso->filename = (const char *) strdup (name);
  setup_strings (&dso->strings);
  setup_lines (&dso->lines);
  return dso;

error_out:
  if (dso)
    {
      free ((char *) dso->filename);
      destroy_strings (&dso->strings);
      destroy_lines (&dso->lines);
      free (dso);
    }
  if (elf)
    elf_end (elf);
  if (fd != -1)
    close (fd);
  return NULL;
}

static const pgpHashAlgo algorithms[] = { PGPHASHALGO_MD5,
  PGPHASHALGO_SHA1, PGPHASHALGO_SHA256, PGPHASHALGO_SHA384, PGPHASHALGO_SHA512 };

/* Compute a fresh build ID bit-string from the editted file contents.  */
static void
handle_build_id (DSO *dso, Elf_Data *build_id,
		 size_t build_id_offset, size_t build_id_size)
{
  DIGEST_CTX ctx;
  pgpHashAlgo algorithm;
  int i = sizeof(algorithms)/sizeof(algorithms[0]);
  void *digest = NULL;
  size_t len;

  while (i-- > 0)
    {
      algorithm = algorithms[i];
      if (rpmDigestLength(algorithm) == build_id_size)
	break;
    }
  if (i < 0)
    {
      fprintf (stderr, "Cannot handle %Zu-byte build ID\n", build_id_size);
      exit (1);
    }

  if (no_recompute_build_id
      || (! dirty_elf && build_id_seed == NULL))
    goto print;

  /* Clear the old bits so they do not affect the new hash.  */
  memset ((char *) build_id->d_buf + build_id_offset, 0, build_id_size);

  ctx = rpmDigestInit(algorithm, 0);

  /* If a seed string was given use it to prime the hash.  */
  if (build_id_seed != NULL)
    rpmDigestUpdate(ctx, build_id_seed, strlen (build_id_seed));

  /* Slurp the relevant header bits and section contents and feed them
     into the hash function.  The only bits we ignore are the offset
     fields in ehdr and shdrs, since the semantically identical ELF file
     could be written differently if it doesn't change the phdr layout.
     We always use the GElf (i.e. Elf64) formats for the bits to hash
     since it is convenient.  It doesn't matter whether this is an Elf32
     or Elf64 object, only that we are consistent in what bits feed the
     hash so it comes out the same for the same file contents.  */
  {
    union
    {
      GElf_Ehdr ehdr;
      GElf_Phdr phdr;
      GElf_Shdr shdr;
    } u;
    Elf_Data x = { .d_version = EV_CURRENT, .d_buf = &u };

    x.d_type = ELF_T_EHDR;
    x.d_size = sizeof u.ehdr;
    u.ehdr = dso->ehdr;
    u.ehdr.e_phoff = u.ehdr.e_shoff = 0;
    if (elf64_xlatetom (&x, &x, dso->ehdr.e_ident[EI_DATA]) == NULL)
      {
      bad:
	fprintf (stderr, "Failed to compute header checksum: %s\n",
		 elf_errmsg (elf_errno ()));
	exit (1);
      }

    x.d_type = ELF_T_PHDR;
    x.d_size = sizeof u.phdr;
    for (i = 0; i < dso->ehdr.e_phnum; ++i)
      {
	if (gelf_getphdr (dso->elf, i, &u.phdr) == NULL)
	  goto bad;
	if (elf64_xlatetom (&x, &x, dso->ehdr.e_ident[EI_DATA]) == NULL)
	  goto bad;
	rpmDigestUpdate(ctx, x.d_buf, x.d_size);
      }

    x.d_type = ELF_T_SHDR;
    x.d_size = sizeof u.shdr;
    for (i = 0; i < dso->ehdr.e_shnum; ++i)
      if (dso->scn[i] != NULL)
	{
	  u.shdr = dso->shdr[i];
	  u.shdr.sh_offset = 0;
	  if (elf64_xlatetom (&x, &x, dso->ehdr.e_ident[EI_DATA]) == NULL)
	    goto bad;
	  rpmDigestUpdate(ctx, x.d_buf, x.d_size);

	  if (u.shdr.sh_type != SHT_NOBITS)
	    {
	      Elf_Data *d = elf_getdata (dso->scn[i], NULL);
	      if (d == NULL)
		goto bad;
	      rpmDigestUpdate(ctx, d->d_buf, d->d_size);
	    }
	}
  }

  rpmDigestFinal(ctx, &digest, &len, 0);
  memcpy((unsigned char *)build_id->d_buf + build_id_offset, digest, build_id_size);
  free(digest);

  elf_flagdata (build_id, ELF_C_SET, ELF_F_DIRTY);

 print:
  /* Now format the build ID bits in hex to print out.  */
  {
    const uint8_t * id = (uint8_t *)build_id->d_buf + build_id_offset;
    char *hex = pgpHexStr(id, build_id_size);
    puts (hex);
    free(hex);
  }
}

int
main (int argc, char *argv[])
{
  DSO *dso;
  int fd, i;
  const char *file;
  poptContext optCon;   /* context for parsing command-line options */
  int nextopt;
  const char **args;
  struct stat stat_buf;
  Elf_Data *build_id = NULL;
  size_t build_id_offset = 0, build_id_size = 0;

  optCon = poptGetContext("debugedit", argc, (const char **)argv, optionsTable, 0);

  while ((nextopt = poptGetNextOpt (optCon)) > 0 || nextopt == POPT_ERROR_BADOPT)
    /* do nothing */ ;

  if (nextopt != -1)
    {
      fprintf (stderr, "Error on option %s: %s.\nRun '%s --help' to see a full list of available command line options.\n",
	      poptBadOption (optCon, 0),
	      poptStrerror (nextopt),
	      argv[0]);
      exit (1);
    }

  if (show_version)
    {
      printf("RPM debugedit %s\n", VERSION);
      exit(EXIT_SUCCESS);
    }

  args = poptGetArgs (optCon);
  if (args == NULL || args[0] == NULL || args[1] != NULL)
    {
      poptPrintHelp(optCon, stdout, 0);
      exit (1);
    }

  if (dest_dir != NULL)
    {
      if (base_dir == NULL)
	{
	  fprintf (stderr, "You must specify a base dir if you specify a dest dir\n");
	  exit (1);
	}
    }

  if (build_id_seed != NULL && do_build_id == 0)
    {
      fprintf (stderr, "--build-id-seed (-s) needs --build-id (-i)\n");
      exit (1);
    }

  if (build_id_seed != NULL && strlen (build_id_seed) < 1)
    {
      fprintf (stderr,
	       "--build-id-seed (-s) string should be at least 1 char\n");
      exit (1);
    }

  /* Ensure clean paths, users can muck with these. Also removes any
     trailing '/' from the paths. */
  if (base_dir)
    canonicalize_path(base_dir, base_dir);
  if (dest_dir)
    canonicalize_path(dest_dir, dest_dir);

  if (list_file != NULL)
    {
      list_file_fd = open (list_file, O_WRONLY|O_CREAT|O_APPEND, 0644);
    }

  file = args[0];

  if (elf_version(EV_CURRENT) == EV_NONE)
    {
      fprintf (stderr, "library out of date\n");
      exit (1);
    }

  if (stat(file, &stat_buf) < 0)
    {
      fprintf (stderr, "Failed to open input file '%s': %s\n", file, strerror(errno));
      exit (1);
    }

  /* Make sure we can read and write */
  chmod (file, stat_buf.st_mode | S_IRUSR | S_IWUSR);

  fd = open (file, O_RDWR);
  if (fd < 0)
    {
      fprintf (stderr, "Failed to open input file '%s': %s\n", file, strerror(errno));
      exit (1);
    }

  dso = fdopen_dso (fd, file);
  if (dso == NULL)
    exit (1);

  for (i = 1; i < dso->ehdr.e_shnum; i++)
    {
      const char *name;

      switch (dso->shdr[i].sh_type)
	{
	case SHT_PROGBITS:
	  name = strptr (dso, dso->ehdr.e_shstrndx, dso->shdr[i].sh_name);
	  /* TODO: Handle stabs */
	  if (strcmp (name, ".stab") == 0)
	    {
	      fprintf (stderr, "Stabs debuginfo not supported: %s\n", file);
	      break;
	    }
	  if (strcmp (name, ".debug_info") == 0)
	    edit_dwarf2 (dso);

	  break;
	case SHT_NOTE:
	  if (do_build_id
	      && build_id == 0 && (dso->shdr[i].sh_flags & SHF_ALLOC))
	    {
	      /* Look for a build-ID note here.  */
	      size_t off = 0;
	      GElf_Nhdr nhdr;
	      size_t name_off;
	      size_t desc_off;
	      Elf_Data *data = elf_getdata (elf_getscn (dso->elf, i), NULL);
	      while ((off = gelf_getnote (data, off,
					  &nhdr, &name_off, &desc_off)) > 0)
		if (nhdr.n_type == NT_GNU_BUILD_ID
		    && nhdr.n_namesz == sizeof "GNU"
		    && (memcmp ((char *)data->d_buf + name_off, "GNU",
				sizeof "GNU") == 0))
		  {
		    build_id = data;
		    build_id_offset = desc_off;
		    build_id_size = nhdr.n_descsz;
		  }
	    }
	  break;
	default:
	  break;
	}
    }

  /* Normally we only need to explicitly update the section headers
     and data when any section data changed size. But because of a bug
     in elfutils before 0.169 we will have to update and write out all
     section data if any data has changed (when ELF_F_LAYOUT was
     set). https://sourceware.org/bugzilla/show_bug.cgi?id=21199 */
  bool need_update = need_strp_update || need_stmt_update;

#if !_ELFUTILS_PREREQ (0, 169)
  /* string replacements or build_id updates don't change section size. */
  need_update = (need_update
		 || need_string_replacement
		 || (do_build_id && build_id != NULL));
#endif

  /* We might have changed the size of some debug sections. If so make
     sure the section headers are updated and the data offsets are
     correct. We set ELF_F_LAYOUT above because we don't want libelf
     to move any allocated sections around itself if there are any
     phdrs. Which means we are responsible for setting the section size
     and offset fields. Plus the shdr offsets. We don't want to change
     anything for the phdrs allocated sections. Keep the offset of
     allocated sections so they are at the same place in the file. Add
     unallocated ones after the allocated ones. */
  if (dso->phnum != 0 && need_update)
    {
      Elf *elf = dso->elf;
      GElf_Off last_offset;
      /* We position everything after the phdrs (which normally would
	 be at the start of the ELF file after the ELF header. */
      last_offset = (dso->ehdr.e_phoff + gelf_fsize (elf, ELF_T_PHDR,
						     dso->phnum, EV_CURRENT));

      /* First find the last allocated section.  */
      Elf_Scn *scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    error (1, 0, "Couldn't get shdr: %s\n", elf_errmsg (-1));

	  /* Any sections we have changed aren't allocated sections,
	     so we don't need to lookup any changed section sizes. */
	  if ((shdr->sh_flags & SHF_ALLOC) != 0)
	    {
	      GElf_Off off = shdr->sh_offset + (shdr->sh_type != SHT_NOBITS
						? shdr->sh_size : 0);
	      if (last_offset < off)
		last_offset = off;
	    }
	}

      /* Now adjust any sizes and offsets for the unallocated sections. */
      scn = NULL;
      while ((scn = elf_nextscn (elf, scn)) != NULL)
	{
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	  if (shdr == NULL)
	    error (1, 0, "Couldn't get shdr: %s\n", elf_errmsg (-1));

	  /* A bug in elfutils before 0.169 means we have to write out
	     all section data, even when nothing changed.
	     https://sourceware.org/bugzilla/show_bug.cgi?id=21199 */
#if !_ELFUTILS_PREREQ (0, 169)
	  if (shdr->sh_type != SHT_NOBITS)
	    {
	      Elf_Data *d = elf_getdata (scn, NULL);
	      elf_flagdata (d, ELF_C_SET, ELF_F_DIRTY);
	    }
#endif
	  if ((shdr->sh_flags & SHF_ALLOC) == 0)
	    {
	      GElf_Off sec_offset = shdr->sh_offset;
	      GElf_Xword sec_size = shdr->sh_size;

	      /* We might have changed the size (and content) of the
		 debug_str or debug_line section. */
	      size_t secnum = elf_ndxscn (scn);
	      if (secnum == debug_sections[DEBUG_STR].sec)
		sec_size = debug_sections[DEBUG_STR].size;
	      if (secnum == debug_sections[DEBUG_LINE].sec)
		sec_size = debug_sections[DEBUG_LINE].size;

	      /* Zero means one.  No alignment constraints.  */
	      size_t addralign = shdr->sh_addralign ?: 1;
	      last_offset = (last_offset + addralign - 1) & ~(addralign - 1);
	      sec_offset = last_offset;
	      if (shdr->sh_type != SHT_NOBITS)
		last_offset += sec_size;

	      if (shdr->sh_size != sec_size
		  || shdr->sh_offset != sec_offset)
		{
		  /* Make sure unchanged section data is written out
		     at the new location. */
		  if (shdr->sh_offset != sec_offset
		      && shdr->sh_type != SHT_NOBITS)
		    {
		      Elf_Data *d = elf_getdata (scn, NULL);
		      elf_flagdata (d, ELF_C_SET, ELF_F_DIRTY);
		    }

		  shdr->sh_size = sec_size;
		  shdr->sh_offset = sec_offset;
		  if (gelf_update_shdr (scn, shdr) == 0)
		    error (1, 0, "Couldn't update shdr: %s\n",
			   elf_errmsg (-1));
		}
	    }
	}

      /* Position the shdrs after the last (unallocated) section.  */
      const size_t offsize = gelf_fsize (elf, ELF_T_OFF, 1, EV_CURRENT);
      GElf_Off new_offset = ((last_offset + offsize - 1)
			     & ~((GElf_Off) (offsize - 1)));
      if (dso->ehdr.e_shoff != new_offset)
	{
	  dso->ehdr.e_shoff = new_offset;
	  if (gelf_update_ehdr (elf, &dso->ehdr) == 0)
	    error (1, 0, "Couldn't update ehdr: %s\n", elf_errmsg (-1));
	}
    }

  if (elf_update (dso->elf, ELF_C_NULL) < 0)
    {
      fprintf (stderr, "Failed to update file: %s\n",
	       elf_errmsg (elf_errno ()));
      exit (1);
    }

  if (do_build_id && build_id != NULL)
    handle_build_id (dso, build_id, build_id_offset, build_id_size);

  if (elf_update (dso->elf, ELF_C_WRITE) < 0)
    {
      fprintf (stderr, "Failed to write file: %s\n", elf_errmsg (elf_errno()));
      exit (1);
    }
  if (elf_end (dso->elf) < 0)
    {
      fprintf (stderr, "elf_end failed: %s\n", elf_errmsg (elf_errno()));
      exit (1);
    }
  close (fd);

  /* Restore old access rights */
  chmod (file, stat_buf.st_mode);

  free ((char *) dso->filename);
  destroy_strings (&dso->strings);
  destroy_lines (&dso->lines);
  free (dso);

  /* In case there were multiple (COMDAT) .debug_macro sections,
     free them.  */
  struct debug_section *macro_sec = &debug_sections[DEBUG_MACRO];
  macro_sec = macro_sec->next;
  while (macro_sec != NULL)
    {
      struct debug_section *next = macro_sec->next;
      free (macro_sec);
      macro_sec = next;
    }

  poptFreeContext (optCon);

  return 0;
}
