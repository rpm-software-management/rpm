/* Accumulation of various pieces of knowledge about ELF.
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

#ifndef _ELF_KNOWLEDGE_H
#define _ELF_KNOWLEDGE_H	1


/* Test whether a section can be stripped or not.  */
#define SECTION_STRIP_P(shdr, name, remove_comment) \
  (/* Sections which are allocated are not removed.  */			      \
   ((shdr)->sh_flags & SHF_ALLOC) == 0					      \
   /* We never remove .note sections.  */				      \
   && (shdr)->sh_type != SHT_NOTE					      \
   /* We remove .comment sections only if explicitly told to do so.  */	      \
   && ((shdr)->sh_type != SHT_PROGBITS					      \
       || (remove_comment)						      \
       || strcmp (name, ".comment") != 0)				      \
   /* So far we do not remove any of the non-standard sections.		      \
      XXX Maybe in future.  */						      \
   && (shdr)->sh_type < SHT_NUM)


/* Test whether `sh_info' field in section header contains a section
   index.  There are two kinds of sections doing this:

   - the sections containing relocation information reference in this
     field the section to which the relocations apply;

   - section with the SHF_INFO_LINK flag set to signal that `sh_info'
     references a section.  This allows correct handling of unknown
     sections.  */
#define SH_INFO_LINK_P(Shdr) \
  ((Shdr)->sh_type == SHT_REL || (Shdr)->sh_type == SHT_RELA		      \
   || ((Shdr)->sh_flags & SHF_INFO_LINK) != 0)


/* When combining ELF section flags we must distinguish two kinds:

   - flags which cause problem if not added to the result even if not
     present in all input sections

   - flags which cause problem if added to the result if not present
     in all input sections

   The following definition is for the general case.  There might be
   machine specific extensions.  */
#define SH_FLAGS_COMBINE(Flags1, Flags2) \
  (((Flags1 | Flags2)							      \
    & (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_LINK_ORDER		      \
       | SHF_OS_NONCONFORMING | SHF_GROUP))				      \
   | (Flags1 & Flags2 & (SHF_MERGE | SHF_STRINGS | SHF_INFO_LINK)))

/* Similar macro: return the bits of the flags which necessarily must
   match if two sections are automatically combined.  Sections still
   can be forcefully combined in which case SH_FLAGS_COMBINE can be
   used to determine the combined flags.  */
#define SH_FLAGS_IMPORTANT(Flags) \
  ((Flags) & ~((GElf_Xword) 0 | SHF_LINK_ORDER | SHF_OS_NONCONFORMING))

#endif	/* elf-knowledge.h */
