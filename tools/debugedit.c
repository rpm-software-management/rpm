/* Copyright (C) 2001, 2002, 2003, 2005, 2007 Red Hat, Inc.
   Written by Alexander Larsson <alexl@redhat.com>, 2002
   Based on code by Jakub Jelinek <jakub@redhat.com>, 2001.

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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <popt.h>

#include <gelf.h>
#include <dwarf.h>

#include <beecrypt/beecrypt.h>

#include "hashtab.h"

#define DW_TAG_partial_unit 0x3c

char *base_dir = NULL;
char *dest_dir = NULL;
char *list_file = NULL;
int list_file_fd = -1;
int do_build_id = 0;

typedef unsigned int uint_32;
typedef unsigned short uint_16;

typedef struct
{
  Elf *elf;
  GElf_Ehdr ehdr;
  Elf_Scn **scn;
  const char *filename;
  int lastscn;
  GElf_Shdr shdr[0];
} DSO;

typedef struct
{
  unsigned char *ptr;
  uint_32 addend;
} REL;

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

static uint_16 (*do_read_16) (unsigned char *ptr);
static uint_32 (*do_read_32) (unsigned char *ptr);
static void (*write_32) (unsigned char *ptr, GElf_Addr val);

static int ptr_size;

static inline uint_16
buf_read_ule16 (unsigned char *data)
{
  return data[0] | (data[1] << 8);
}

static inline uint_16
buf_read_ube16 (unsigned char *data)
{
  return data[1] | (data[0] << 8);
}

static inline uint_32
buf_read_ule32 (unsigned char *data)
{
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static inline uint_32
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
      while ((data = elf_rawdata (scn, data)) != NULL)
	{
	  if (data->d_buf
	      && offset >= data->d_off
	      && offset < data->d_off + data->d_size)
	    return (const char *) data->d_buf + (offset - data->d_off);
	}
    }

  return NULL;
}


#define read_1(ptr) *ptr++

#define read_16(ptr) ({					\
  uint_16 ret = do_read_16 (ptr);			\
  ptr += 2;						\
  ret;							\
})

#define read_32(ptr) ({					\
  uint_32 ret = do_read_32 (ptr);			\
  ptr += 4;						\
  ret;							\
})

REL *relptr, *relend;
int reltype;

#define do_read_32_relocated(ptr) ({			\
  uint_32 dret = do_read_32 (ptr);			\
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
  uint_32 ret = do_read_32_relocated (ptr);		\
  ptr += 4;						\
  ret;							\
})

static void
dwarf2_write_le32 (unsigned char *p, GElf_Addr val)
{
  uint_32 v = (uint_32) val;

  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}


static void
dwarf2_write_be32 (unsigned char *p, GElf_Addr val)
{
  uint_32 v = (uint_32) val;

  p[3] = v;
  p[2] = v >> 8;
  p[1] = v >> 16;
  p[0] = v >> 24;
}

static struct
  {
    const char *name;
    unsigned char *data;
    Elf_Data *elf_data;
    size_t size;
    int sec, relsec;
  } debug_sections[] =
  {
#define DEBUG_INFO	0
#define DEBUG_ABBREV	1
#define DEBUG_LINE	2
#define DEBUG_ARANGES	3
#define DEBUG_PUBNAMES	4
#define DEBUG_MACINFO	5
#define DEBUG_LOC	6
#define DEBUG_STR	7
#define DEBUG_FRAME	8
#define DEBUG_RANGES	9
    { ".debug_info", NULL, NULL, 0, 0, 0 },
    { ".debug_abbrev", NULL, NULL, 0, 0, 0 },
    { ".debug_line", NULL, NULL, 0, 0, 0 },
    { ".debug_aranges", NULL, NULL, 0, 0, 0 },
    { ".debug_pubnames", NULL, NULL, 0, 0, 0 },
    { ".debug_macinfo", NULL, NULL, 0, 0, 0 },
    { ".debug_loc", NULL, NULL, 0, 0, 0 },
    { ".debug_str", NULL, NULL, 0, 0, 0 },
    { ".debug_frame", NULL, NULL, 0, 0, 0 },
    { ".debug_ranges", NULL, NULL, 0, 0, 0 },
    { NULL, NULL, NULL, 0, 0, 0 }
  };

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
	  error (0, 0, "%s: Duplicate DWARF-2 abbreviation %d", dso->filename,
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
	  if (form == 2 || form > DW_FORM_indirect)
	    {
	      error (0, 0, "%s: Unknown DWARF-2 DW_FORM_%d", dso->filename, form);
	      htab_delete (h);
	      return NULL;
	    }

	  t->attr[t->nattr].attr = attr;
	  t->attr[t->nattr++].form = form;
        }
      if (read_uleb128 (ptr) != 0)
        {
	  error (0, 0, "%s: DWARF-2 abbreviation does not end with 2 zeros",
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
  const char *sroot;
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
  sroot = s;

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

static int
has_prefix (const char  *str,
	    const char  *prefix)
{
  int str_len;
  int prefix_len;
  
  str_len = strlen (str);
  prefix_len = strlen (prefix);

  if (str_len < prefix_len)
    return 0;
  
  return strncmp (str, prefix, prefix_len) == 0;
}

static int
edit_dwarf2_line (DSO *dso, uint_32 off, char *comp_dir, int phase)
{
  unsigned char *ptr = debug_sections[DEBUG_LINE].data, *dir;
  unsigned char **dirt;
  unsigned char *endsec = ptr + debug_sections[DEBUG_LINE].size;
  unsigned char *endcu, *endprol;
  unsigned char opcode_base;
  uint_32 value, dirt_cnt;
  size_t comp_dir_len = strlen (comp_dir);
  size_t abs_file_cnt = 0, abs_dir_cnt = 0;

  if (phase != 0)
    return 0;
  
  ptr += off;
  
  endcu = ptr + 4;
  endcu += read_32 (ptr);
  if (endcu == ptr + 0xffffffff)
    {
      error (0, 0, "%s: 64-bit DWARF not supported", dso->filename);
      return 1;
    }

  if (endcu > endsec)
    {
      error (0, 0, "%s: .debug_line CU does not fit into section",
	     dso->filename);
      return 1;
    }

  value = read_16 (ptr);
  if (value != 2)
    {
      error (0, 0, "%s: DWARF version %d unhandled", dso->filename,
	     value);
      return 1;
    }
  
  endprol = ptr + 4;
  endprol += read_32 (ptr);
  if (endprol > endcu)
    {
      error (0, 0, "%s: .debug_line CU prologue does not fit into CU",
	     dso->filename);
      return 1;
    }
  
  opcode_base = ptr[4];
  ptr = dir = ptr + 4 + opcode_base;
  
  /* dir table: */
  value = 1;
  while (*ptr != 0)
    {
      ptr = strchr (ptr, 0) + 1;
      ++value;
    }

  dirt = (unsigned char **) alloca (value * sizeof (unsigned char *));
  dirt[0] = ".";
  dirt_cnt = 1;
  ptr = dir;
  while (*ptr != 0)
    {
      dirt[dirt_cnt++] = ptr;
      ptr = strchr (ptr, 0) + 1;
    }
  ptr++;

  /* file table: */
  while (*ptr != 0)
    {
      char *s, *file;
      size_t file_len, dir_len;

      file = ptr;
      ptr = strchr (ptr, 0) + 1;
      value = read_uleb128 (ptr);

      if (value >= dirt_cnt)
	{
	  error (0, 0, "%s: Wrong directory table index %u",
		 dso->filename, value);
	  return 1;
	}
      file_len = strlen (file);
      dir_len = strlen ((char *)dirt[value]);
      s = malloc (comp_dir_len + 1 + file_len + 1 + dir_len + 1);
      if (s == NULL)
	{
	  error (0, ENOMEM, "%s: Reading file table", dso->filename);
	  return 1;
	}
      if (*file == '/')
	{
	  memcpy (s, file, file_len + 1);
	  if (dest_dir && has_prefix (file, base_dir))
	    ++abs_file_cnt;
	}
      else if (*dirt[value] == '/')
	{
	  memcpy (s, dirt[value], dir_len);
	  s[dir_len] = '/';
	  memcpy (s + dir_len + 1, file, file_len + 1);
	}
      else
	{
	  memcpy (s, comp_dir, comp_dir_len);
	  s[comp_dir_len] = '/';
	  memcpy (s + comp_dir_len + 1, dirt[value], dir_len);
	  s[comp_dir_len + 1 + dir_len] = '/';
	  memcpy (s + comp_dir_len + 1 + dir_len + 1, file, file_len + 1);
	}
      canonicalize_path (s, s);
      if (base_dir == NULL ||
	  has_prefix (s, base_dir))
	{
	  char *p;
	  size_t size;
	  ssize_t ret;
	  if (base_dir)
	    p = s + strlen (base_dir);
	  else
	    p = s;
	  
	  if (list_file_fd != -1)
	    {
	      size = strlen (p) + 1;
	      while (size > 0)
		{
		  ret = write (list_file_fd, p, size);
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
  ++ptr;
  
  if (dest_dir)
    {
      unsigned char *srcptr, *buf = NULL;
      size_t base_len = strlen (base_dir);
      size_t dest_len = strlen (dest_dir);
      size_t shrank = 0;

      if (dest_len == base_len)
	abs_file_cnt = 0;
      if (abs_file_cnt)
	{
	  srcptr = buf = malloc (ptr - dir);
	  memcpy (srcptr, dir, ptr - dir);
	  ptr = dir;
	}
      else
	ptr = srcptr = dir;
      unsigned char *srcstart=srcptr;
      while (*srcptr != 0)
	{
	  size_t len = strlen ((char *)srcptr) + 1;
	  const unsigned char *readptr = srcptr;

	  if (*srcptr == '/' && has_prefix ((char *)srcptr, base_dir))
	    {
	      if (dest_len < base_len)
		  ++abs_dir_cnt;
	      memcpy (ptr, dest_dir, dest_len);
	      ptr += dest_len;
	      readptr += base_len;
		}
	  srcptr += len;

	  shrank += srcptr - readptr;
	  canonicalize_path ((char *)readptr, (char *)ptr);
	  len = strlen ((char *)ptr) + 1;
	  shrank -= len;
	  ptr += len;

	      elf_flagdata (debug_sections[DEBUG_STR].elf_data,
			    ELF_C_SET, ELF_F_DIRTY);
	    }

      if (shrank > 0)
	{
	  if (--shrank == 0)
	    error (EXIT_FAILURE, 0,
		   "canonicalization unexpectedly shrank by one character");
	  memset (ptr, 'X', shrank);
	  ptr += shrank;
	  *ptr++ = '\0';
	}

      if (abs_dir_cnt + abs_file_cnt != 0)
	{
	  size_t len = (abs_dir_cnt + abs_file_cnt) * (base_len - dest_len);

	  if (len == 1)
	    error (EXIT_FAILURE, 0, "-b arg has to be either the same length as -d arg, or more than 1 char shorter");
	  memset (ptr, 'X', len - 1);
	  ptr += len - 1;
	  *ptr++ = '\0';
	}
      *ptr++ = '\0';
      ++srcptr;

      while (*srcptr != 0)
	{
	  size_t len = strlen ((char *)srcptr) + 1;

	  if (*srcptr == '/' && has_prefix ((char *)srcptr, base_dir))
	    {
	      memcpy (ptr, dest_dir, dest_len);
	      if (dest_len < base_len)
		{
		  memmove (ptr + dest_len, srcptr + base_len,
			   len - base_len);
		  ptr += dest_len - base_len;
		}
	      elf_flagdata (debug_sections[DEBUG_STR].elf_data,
			    ELF_C_SET, ELF_F_DIRTY);
	    }
	  else if (ptr != srcptr)
	    memmove (ptr, srcptr, len);
	  srcptr += len;
	  ptr += len;
	  dir = srcptr;
	  read_uleb128 (srcptr);
	  read_uleb128 (srcptr);
	  read_uleb128 (srcptr);
	  if (ptr != dir)
	    memmove (ptr, dir, srcptr - dir);
	  ptr += srcptr - dir;
	}
      *ptr = '\0';
      free (buf);
    }
  return 0;
}



static unsigned char *
edit_attributes (DSO *dso, unsigned char *ptr, struct abbrev_tag *t, int phase)
{
  int i;
  uint_32 list_offs;
  int found_list_offs;
  unsigned char *comp_dir;
  
  comp_dir = NULL;
  list_offs = 0;
  found_list_offs = 0;
  for (i = 0; i < t->nattr; ++i)
    {
      uint_32 form = t->attr[i].form;
      uint_32 len = 0;
      int base_len, dest_len;
      

      while (1)
	{
	  if (t->attr[i].attr == DW_AT_stmt_list)
	    {
	      if (form == DW_FORM_data4)
		{
		  list_offs = do_read_32_relocated (ptr);
		  found_list_offs = 1;
		}
	    }

	  if (t->attr[i].attr == DW_AT_comp_dir)
	  {
	      if ( form == DW_FORM_string )
	      {
		  free (comp_dir);
		  comp_dir = strdup ((char *)ptr);
		  
		  if (phase == 1 && dest_dir && has_prefix ((char *)ptr, base_dir))
		  {
		      base_len = strlen (base_dir);
		      dest_len = strlen (dest_dir);
		      
		      memcpy (ptr, dest_dir, dest_len);
		      if (dest_len < base_len)
		      {
			  memset(ptr + dest_len, '/',
				 base_len - dest_len);
			  
		      }
		      elf_flagdata (debug_sections[DEBUG_INFO].elf_data,
				    ELF_C_SET, ELF_F_DIRTY);
		  }
	      }
	  
	      else if (form == DW_FORM_strp &&
		       debug_sections[DEBUG_STR].data)
	      {
		  char *dir;

		  dir = debug_sections[DEBUG_STR].data
		      + do_read_32_relocated (ptr);

		  free (comp_dir);
		  comp_dir = strdup (dir);

		  if (phase == 1 && dest_dir && has_prefix (dir, base_dir))
		  {
		      base_len = strlen (base_dir);
		      dest_len = strlen (dest_dir);
		  
		      memcpy (dir, dest_dir, dest_len);
		      if (dest_len < base_len)
		      {
			  memmove (dir + dest_len, dir + base_len,
				   strlen (dir + base_len) + 1);
		      }
		      elf_flagdata (debug_sections[DEBUG_STR].elf_data,
				    ELF_C_SET, ELF_F_DIRTY);
		  }
	      }
	  }
	  else if ((t->tag == DW_TAG_compile_unit
		    || t->tag == DW_TAG_partial_unit)
		   && t->attr[i].attr == DW_AT_name
		   && form == DW_FORM_strp
		   && debug_sections[DEBUG_STR].data)
	    {
	      char *name;
	      
	      name = debug_sections[DEBUG_STR].data
		     + do_read_32_relocated (ptr);
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

	      if (phase == 1 && dest_dir && has_prefix (name, base_dir))
		{
		  base_len = strlen (base_dir);
		  dest_len = strlen (dest_dir);
		  
		  memcpy (name, dest_dir, dest_len);
		  if (dest_len < base_len)
		    {
		      memmove (name + dest_len, name + base_len,
			       strlen (name + base_len) + 1);
		    }
		  elf_flagdata (debug_sections[DEBUG_STR].elf_data,
				ELF_C_SET, ELF_F_DIRTY);
		}
	    }

	  switch (form)
	    {
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
	      ptr += 4;
	      break;
	    case DW_FORM_ref8:
	    case DW_FORM_data8:
	      ptr += 8;
	      break;
	    case DW_FORM_sdata:
	    case DW_FORM_ref_udata:
	    case DW_FORM_udata:
	      read_uleb128 (ptr);
	      break;
	    case DW_FORM_ref_addr:
	    case DW_FORM_strp:
	      ptr += 4;
	      break;
	    case DW_FORM_string:
	      ptr = strchr (ptr, '\0') + 1;
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
	      len = read_uleb128 (ptr);
	      form = DW_FORM_block1;
	      assert (len < UINT_MAX);
	      break;
	    default:
	      error (0, 0, "%s: Unknown DWARF-2 DW_FORM_%d", dso->filename,
		     form);
	      return NULL;
	    }

	  if (form == DW_FORM_block1)
	    ptr += len;
	  
	  break;
	}
    }
  if (found_list_offs && comp_dir)
    edit_dwarf2_line (dso, list_offs, comp_dir, phase);

  free (comp_dir);

  return ptr;
}

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
		  if (debug_sections[j].data)
		    {
		      error (0, 0, "%s: Found two copies of %s section",
			     dso->filename, name);
		      return 1;
		    }

		  scn = dso->scn[i]; 
		  data = elf_rawdata (scn, NULL);
		  assert (data != NULL && data->d_buf != NULL);
		  assert (elf_rawdata (scn, data) == NULL);
		  assert (data->d_off == 0);
		  assert (data->d_size == dso->shdr[i].sh_size);
		  debug_sections[j].data = data->d_buf;
		  debug_sections[j].elf_data = data;
		  debug_sections[j].size = data->d_size;
		  debug_sections[j].sec = i;
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
		  debug_sections[j].relsec = i;
		  break;
		}
	  }
      }

  if (dso->ehdr.e_ident[EI_DATA] == ELFDATA2LSB)
    {
      do_read_16 = buf_read_ule16;
      do_read_32 = buf_read_ule32;
      write_32 = dwarf2_write_le32;
    }
  else if (dso->ehdr.e_ident[EI_DATA] == ELFDATA2MSB)
    {
      do_read_16 = buf_read_ube16;
      do_read_32 = buf_read_ube32;
      write_32 = dwarf2_write_be32;
    }
  else
    {
      error (0, 0, "%s: Wrong ELF data enconding", dso->filename);
      return 1;
    }

  if (debug_sections[DEBUG_INFO].data != NULL)
    {
      unsigned char *ptr, *endcu, *endsec;
      uint_32 value;
      htab_t abbrev;
      struct abbrev_tag tag, *t;
      int phase;
      REL *relbuf = NULL;

      if (debug_sections[DEBUG_INFO].relsec)
	{
	  int ndx, maxndx;
	  GElf_Rel rel;
	  GElf_Rela rela;
	  GElf_Sym sym;
	  GElf_Addr base = dso->shdr[debug_sections[DEBUG_INFO].sec].sh_addr;
	  Elf_Data *symdata = NULL;
	  int rtype;

	  i = debug_sections[DEBUG_INFO].relsec;
	  scn = dso->scn[i]; 
	  data = elf_getdata (scn, NULL);
	  assert (data != NULL && data->d_buf != NULL);
	  assert (elf_getdata (scn, data) == NULL);
	  assert (data->d_off == 0);
	  assert (data->d_size == dso->shdr[i].sh_size);
	  maxndx = dso->shdr[i].sh_size / dso->shdr[i].sh_entsize;
	  relbuf = malloc (maxndx * sizeof (REL));
	  reltype = dso->shdr[i].sh_type;
	  if (relbuf == NULL)
	    error (1, errno, "%s: Could not allocate memory", dso->filename);

	  symdata = elf_getdata (dso->scn[dso->shdr[i].sh_link], NULL);
	  assert (symdata != NULL && symdata->d_buf != NULL);
	  assert (elf_getdata (dso->scn[dso->shdr[i].sh_link], symdata)
		  == NULL);
	  assert (symdata->d_off == 0);
	  assert (symdata->d_size
		  == dso->shdr[dso->shdr[i].sh_link].sh_size);

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
	      /* Relocations against section symbols are uninteresting
		 in REL.  */
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
		default:
		fail:
		  error (1, 0, "%s: Unhandled relocation %d in .debug_info section",
			 dso->filename, rtype);
		}
	      relend->ptr = debug_sections[DEBUG_INFO].data
			    + (rela.r_offset - base);
	      relend->addend = rela.r_addend;
	      ++relend;
	    }
	  if (relbuf == relend)
	    {
	      free (relbuf);
	      relbuf = NULL;
	      relend = NULL;
	    }
	  else
	    qsort (relbuf, relend - relbuf, sizeof (REL), rel_cmp);
	}

      for (phase = 0; phase < 2; phase++)
	{
	  ptr = debug_sections[DEBUG_INFO].data;
	  relptr = relbuf;
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
	      
	      value = read_16 (ptr);
	      if (value != 2)
		{
		  error (0, 0, "%s: DWARF version %d unhandled", dso->filename,
			 value);
		  return 1;
		}
	      
	      value = read_32_relocated (ptr);
	      if (value >= debug_sections[DEBUG_ABBREV].size)
		{
		  if (debug_sections[DEBUG_ABBREV].data == NULL)
		    error (0, 0, "%s: .debug_abbrev not present", dso->filename);
		  else
		    error (0, 0, "%s: DWARF-2 CU abbrev offset too large",
			   dso->filename);
		  return 1;
		}
	      
	      if (ptr_size == 0)
		{
		  ptr_size = read_1 (ptr);
		  if (ptr_size != 4 && ptr_size != 8)
		    {
		      error (0, 0, "%s: Invalid DWARF-2 pointer size %d",
			     dso->filename, ptr_size);
		      return 1;
		    }
		}
	      else if (read_1 (ptr) != ptr_size)
		{
		  error (0, 0, "%s: DWARF-2 pointer size differs between CUs",
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
		      error (0, 0, "%s: Could not find DWARF-2 abbreviation %d",
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
	}
      free (relbuf);
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

  elf = elf_begin (fd, ELF_C_RDWR_MMAP, NULL);
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

  elf_flagelf (elf, ELF_C_SET, ELF_F_LAYOUT);
  
  memset (dso, 0, sizeof(DSO));
  dso->elf = elf;
  dso->ehdr = ehdr;
  dso->scn = (Elf_Scn **) &dso->shdr[ehdr.e_shnum + 20];

  for (i = 0; i < ehdr.e_shnum; ++i)
    {
      dso->scn[i] = elf_getscn (elf, i);
      gelf_getshdr (dso->scn[i], dso->shdr + i);
    }

  dso->filename = (const char *) strdup (name);
  return dso;

error_out:
  if (dso)
    {
      free ((char *) dso->filename);
      free (dso);
    }
  if (elf)
    elf_end (elf);
  if (fd != -1)
    close (fd);
  return NULL;
}

/* Compute a fresh build ID bit-string from the editted file contents.  */
static void
handle_build_id (DSO *dso, Elf_Data *build_id,
		 size_t build_id_offset, size_t build_id_size)
{
  hashFunctionContext ctx;
  const hashFunction *hf = NULL;
  int i = hashFunctionCount ();

  while (i-- > 0)
    {
      hf = hashFunctionGet (i);
      if (hf != NULL && hf->digestsize == build_id_size)
	break;
    }
  if (hf == NULL)
    {
      fprintf (stderr, "Cannot handle %Zu-byte build ID\n", build_id_size);
      exit (1);
    }

  if (elf_update (dso->elf, ELF_C_NULL) < 0)
    {
      fprintf (stderr, "Failed to update file: %s\n",
	       elf_errmsg (elf_errno ()));
      exit (1);
    }

  /* Clear the old bits so they do not affect the new hash.  */
  memset ((char *) build_id->d_buf + build_id_offset, 0, build_id_offset);

  hashFunctionContextInit (&ctx, hf);

  /* Slurp the relevant header bits and section contents and feed them
     into the hash function.  The only bits we ignore are the offset
     fields in ehdr and shdrs, since the semantically identical ELF file
     could be written differently if it doesn't change the phdr layout.
     We always use the GElf (i.e. Elf64) formats for the bits to hash
     since it is convenient.  It doesn't matter whether this is an Elf32
     or Elf64 object, only that we are consistent in what bits feed the
     hash so it comes out the same for the same file contents.  */
  {
    inline void process (const void *data, size_t size);
    inline void process (const void *data, size_t size)
    {
      memchunk chunk = { .data = (void *) data, .size = size };
      hashFunctionContextUpdateMC (&ctx, &chunk);
    }

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
	process (x.d_buf, x.d_size);
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
	  process (x.d_buf, x.d_size);

	  if (u.shdr.sh_type != SHT_NOBITS)
	    {
	      Elf_Data *d = elf_rawdata (dso->scn[i], NULL);
	      if (d == NULL)
		goto bad;
	      process (d->d_buf, d->d_size);
	    }
	}
  }

  hashFunctionContextDigest (&ctx, (byte *) build_id->d_buf + build_id_offset);
  hashFunctionContextFree (&ctx);

  elf_flagdata (build_id, ELF_C_SET, ELF_F_DIRTY);

  /* Now format the build ID bits in hex to print out.  */
  {
    const unsigned char * id = build_id->d_buf + build_id_offset;
    char hex[build_id_size * 2 + 1];
    int n = snprintf (hex, 3, "%02" PRIx8, id[0]);
    assert (n == 2);
    for (i = 1; i < build_id_size; ++i)
      {
	n = snprintf (&hex[i * 2], 3, "%02" PRIx8, id[i]);
	assert (n == 2);
      }
    puts (hex);
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
  char *p;
  Elf_Data *build_id = NULL;
  size_t build_id_offset = 0, build_id_size = 0;

  optCon = poptGetContext("debugedit", argc, (const char **)argv,
			  optionsTable, 0);
  
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
      if (strlen (dest_dir) > strlen (base_dir))
	{
	  fprintf (stderr, "Only dest dir longer than base dir not supported\n");
	  exit (1);
	}
    }

  /* Make sure there are trailing slashes in dirs */
  if (base_dir != NULL && base_dir[strlen (base_dir)-1] != '/')
    {
      p = malloc (strlen (base_dir) + 2);
      strcpy (p, base_dir);
      strcat (p, "/");
      free (base_dir);
      base_dir = p;
    }
  if (dest_dir != NULL && dest_dir[strlen (dest_dir)-1] != '/')
    {
      p = malloc (strlen (dest_dir) + 2);
      strcpy (p, dest_dir);
      strcat (p, "/");
      free (dest_dir);
      dest_dir = p;
    }
  
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
#if 0
	  if (strcmp (name, ".stab") == 0)
	    edit_stabs (dso, i);
#endif
	  if (strcmp (name, ".debug_info") == 0)
	    edit_dwarf2 (dso);

	  break;
	case SHT_NOTE:
	  if (do_build_id
	      && build_id == NULL && (dso->shdr[i].sh_flags & SHF_ALLOC))
	    {
	      /* Look for a build-ID note here.  */
	      Elf_Data *data = elf_rawdata (elf_getscn (dso->elf, i), NULL);
	      Elf32_Nhdr nh;
	      Elf_Data dst =
		{
		  .d_version = EV_CURRENT, .d_type = ELF_T_NHDR,
		  .d_buf = &nh, .d_size = sizeof nh
		};
	      Elf_Data src = dst;
	      src.d_buf = data->d_buf;
	      assert (sizeof (Elf32_Nhdr) == sizeof (Elf64_Nhdr));
	      while (data->d_buf + data->d_size - src.d_buf > (int) sizeof nh
		     && elf32_xlatetom (&dst, &src, dso->ehdr.e_ident[EI_DATA]))
		{
		  Elf32_Word len = sizeof nh + nh.n_namesz;
		  len = (len + 3) & ~3;

		  if (nh.n_namesz == sizeof "GNU" && nh.n_type == 3
		      && !memcmp (src.d_buf + sizeof nh, "GNU", sizeof "GNU"))
		    {
		      build_id = data;
		      build_id_offset = src.d_buf + len - data->d_buf;
		      build_id_size = nh.n_descsz;
		      break;
		    }

		  len += nh.n_descsz;
		  len = (len + 3) & ~3;
		  src.d_buf += len;
		}
	    }
	  break;
	default:
	  break;
	}
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
  
  poptFreeContext (optCon);

  return 0;
}
