/*
ext_types.h - external representation of ELF data types.
Copyright (C) 1995, 1996 Michael Riepe <michael@stud.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _EXT_TYPES_H
#define _EXT_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Scalar data types
 */
typedef unsigned char __ext_Elf32_Addr	[ELF32_FSZ_ADDR];
typedef unsigned char __ext_Elf32_Half	[ELF32_FSZ_HALF];
typedef unsigned char __ext_Elf32_Off	[ELF32_FSZ_OFF];
typedef unsigned char __ext_Elf32_Sword	[ELF32_FSZ_SWORD];
typedef unsigned char __ext_Elf32_Word	[ELF32_FSZ_WORD];

/*
 * ELF header
 */
typedef struct {
    unsigned char	e_ident[EI_NIDENT];
    __ext_Elf32_Half	e_type;
    __ext_Elf32_Half	e_machine;
    __ext_Elf32_Word	e_version;
    __ext_Elf32_Addr	e_entry;
    __ext_Elf32_Off	e_phoff;
    __ext_Elf32_Off	e_shoff;
    __ext_Elf32_Word	e_flags;
    __ext_Elf32_Half	e_ehsize;
    __ext_Elf32_Half	e_phentsize;
    __ext_Elf32_Half	e_phnum;
    __ext_Elf32_Half	e_shentsize;
    __ext_Elf32_Half	e_shnum;
    __ext_Elf32_Half	e_shstrndx;
} __ext_Elf32_Ehdr;

/*
 * Section header
 */
typedef struct {
    __ext_Elf32_Word	sh_name;
    __ext_Elf32_Word	sh_type;
    __ext_Elf32_Word	sh_flags;
    __ext_Elf32_Addr	sh_addr;
    __ext_Elf32_Off	sh_offset;
    __ext_Elf32_Word	sh_size;
    __ext_Elf32_Word	sh_link;
    __ext_Elf32_Word	sh_info;
    __ext_Elf32_Word	sh_addralign;
    __ext_Elf32_Word	sh_entsize;
} __ext_Elf32_Shdr;

/*
 * Symbol table
 */
typedef struct {
    __ext_Elf32_Word	st_name;
    __ext_Elf32_Addr	st_value;
    __ext_Elf32_Word	st_size;
    unsigned char	st_info;
    unsigned char	st_other;
    __ext_Elf32_Half	st_shndx;
} __ext_Elf32_Sym;

/*
 * Relocation
 */
typedef struct {
    __ext_Elf32_Addr	r_offset;
    __ext_Elf32_Word	r_info;
} __ext_Elf32_Rel;

typedef struct {
    __ext_Elf32_Addr	r_offset;
    __ext_Elf32_Word	r_info;
    __ext_Elf32_Sword	r_addend;
} __ext_Elf32_Rela;

/*
 * Program header
 */
typedef struct {
    __ext_Elf32_Word	p_type;
    __ext_Elf32_Off	p_offset;
    __ext_Elf32_Addr	p_vaddr;
    __ext_Elf32_Addr	p_paddr;
    __ext_Elf32_Word	p_filesz;
    __ext_Elf32_Word	p_memsz;
    __ext_Elf32_Word	p_flags;
    __ext_Elf32_Word	p_align;
} __ext_Elf32_Phdr;

/*
 * Dynamic structure
 */
typedef struct {
    __ext_Elf32_Sword	d_tag;
    union {
	__ext_Elf32_Word	d_val;
	__ext_Elf32_Addr	d_ptr;
    } d_un;
} __ext_Elf32_Dyn;

#ifdef	__cplusplus
}
#endif

#endif /* _EXT_TYPES_H */
