/*
cook.c - read and translate ELF files.
Copyright (C) 1995 - 2001 Michael Riepe <michael@stud.uni-hannover.de>

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

#include <private.h>

#ifndef lint
static const char rcsid[] = "@(#) Id: cook.c,v 1.17 2001/10/07 19:36:02 michael Exp ";
#endif /* lint */

/*@-fullinitblock@*/
const Elf_Scn _elf_scn_init = INIT_SCN;
const Scn_Data _elf_data_init = INIT_DATA;
/*@=fullinitblock@*/

Elf_Type
_elf_scn_type(unsigned t) {
    switch (t) {
	case SHT_DYNAMIC:      return ELF_T_DYN;
	case SHT_DYNSYM:       return ELF_T_SYM;
	case SHT_HASH:         return ELF_T_WORD;
	case SHT_REL:          return ELF_T_REL;
	case SHT_RELA:         return ELF_T_RELA;
	case SHT_SYMTAB:       return ELF_T_SYM;
#if __LIBELF_SYMBOL_VERSIONS
#if __LIBELF_SUN_SYMBOL_VERSIONS
	case SHT_SUNW_verdef:  return ELF_T_VDEF;
	case SHT_SUNW_verneed: return ELF_T_VNEED;
	case SHT_SUNW_versym:  return ELF_T_HALF;
#else /* __LIBELF_SUN_SYMBOL_VERSIONS */
	case SHT_GNU_verdef:   return ELF_T_VDEF;
	case SHT_GNU_verneed:  return ELF_T_VNEED;
	case SHT_GNU_versym:   return ELF_T_HALF;
#endif /* __LIBELF_SUN_SYMBOL_VERSIONS */
#endif /* __LIBELF_SYMBOL_VERSIONS */
    }
    return ELF_T_BYTE;
}

/*
 * Check for overflow on 32-bit systems
 */
#define overflow(a,b,t)	(sizeof(a) < sizeof(t) && (t)(a) != (b))

#define truncerr(t) ((t)==ELF_T_EHDR?ERROR_TRUNC_EHDR:	\
		    ((t)==ELF_T_PHDR?ERROR_TRUNC_PHDR:	\
		    ERROR_INTERNAL))
#define memerr(t)   ((t)==ELF_T_EHDR?ERROR_MEM_EHDR:	\
		    ((t)==ELF_T_PHDR?ERROR_MEM_PHDR:	\
		    ERROR_INTERNAL))

Elf_Data*
_elf_xlatetom(const Elf *elf, Elf_Data *dst, const Elf_Data *src) {
    if (elf->e_class == ELFCLASS32) {
	return elf32_xlatetom(dst, src, elf->e_encoding);
    }
#if __LIBELF64
    else if (elf->e_class == ELFCLASS64) {
	return elf64_xlatetom(dst, src, elf->e_encoding);
    }
#endif /* __LIBELF64 */
    seterr(ERROR_UNIMPLEMENTED);
    return NULL;
}

/*@null@*/
static char*
_elf_item(Elf *elf, Elf_Type type, size_t n, size_t off, int *flag)
	/*@globals _elf_errno @*/
	/*@modifies *elf, *flag, _elf_errno @*/
{
    Elf_Data src, dst;

    *flag = 0;
    elf_assert(n);
    elf_assert(valid_type(type));
    if (off < 0 || off > elf->e_size) {
	seterr(ERROR_OUTSIDE);
	return NULL;
    }

    src.d_type = type;
    src.d_version = elf->e_version;
    src.d_size = n * _fsize(elf->e_class, src.d_version, type);
    elf_assert(src.d_size);
    if (off + src.d_size < off /* modulo overflow */
     || off + src.d_size > elf->e_size) {
	seterr(truncerr(type));
	return NULL;
    }

    dst.d_version = _elf_version;
    dst.d_size = n * _msize(elf->e_class, dst.d_version, type);
    elf_assert(dst.d_size);

    elf_assert(elf->e_data);
    if (elf->e_rawdata != elf->e_data && dst.d_size <= src.d_size) {
	dst.d_buf = elf->e_data + off;
    }
    else if (!(dst.d_buf = malloc(dst.d_size))) {
	seterr(memerr(type));
	return NULL;
    }
    else {
	*flag = 1;
    }

    if (elf->e_rawdata) {
	src.d_buf = elf->e_rawdata + off;
    }
    else {
	src.d_buf = elf->e_data + off;
    }

    if (_elf_xlatetom(elf, &dst, &src)) {
	if (!*flag) {
	    elf->e_cooked = 1;
	}
	return (char*)dst.d_buf;
    }

    if (*flag) {
	free(dst.d_buf);
	*flag = 0;
    }
    return NULL;
}

#undef truncerr
#undef memerr

/*@-uniondef@*/
static int
_elf_cook_file(Elf *elf)
	/*@globals _elf_errno, _elf_data_init, _elf_scn_init @*/
	/*@modifies *elf, _elf_errno, _elf_data_init, _elf_scn_init @*/
{
    size_t num, off, align_addr;
    int flag;

    elf->e_ehdr = _elf_item(elf, ELF_T_EHDR, 1, 0, &flag);
    if (!elf->e_ehdr) {
	return 0;
    }
    if (flag) {
	elf->e_free_ehdr = 1;
    }
    align_addr = _fsize(elf->e_class, elf->e_version, ELF_T_ADDR);
    elf_assert(align_addr);
    if (elf->e_class == ELFCLASS32) {
	num = ((Elf32_Ehdr*)elf->e_ehdr)->e_phnum;
	off = ((Elf32_Ehdr*)elf->e_ehdr)->e_phoff;
    }
#if __LIBELF64
    else if (elf->e_class == ELFCLASS64) {
	num = ((Elf64_Ehdr*)elf->e_ehdr)->e_phnum;
	off = ((Elf64_Ehdr*)elf->e_ehdr)->e_phoff;
	/*
	 * Check for overflow on 32-bit systems
	 */
	if (overflow(off, ((Elf64_Ehdr*)elf->e_ehdr)->e_phoff, Elf64_Off)) {
	    seterr(ERROR_OUTSIDE);
	    return 0;
	}
    }
#endif /* __LIBELF64 */
    else {
	seterr(ERROR_UNIMPLEMENTED);
	return 0;
    }
    if (num && off) {
	if (off % align_addr) {
	    seterr(ERROR_ALIGN_PHDR);
	    return 0;
	}
	elf->e_phdr = _elf_item(elf, ELF_T_PHDR, num, off, &flag);
	if (!elf->e_phdr) {
	    return 0;
	}
	if (flag) {
	    elf->e_free_phdr = 1;
	}
	elf->e_phnum = num;
    }
    if (elf->e_class == ELFCLASS32) {
	num = ((Elf32_Ehdr*)elf->e_ehdr)->e_shnum;
	off = ((Elf32_Ehdr*)elf->e_ehdr)->e_shoff;
    }
#if __LIBELF64
    else if (elf->e_class == ELFCLASS64) {
	num = ((Elf64_Ehdr*)elf->e_ehdr)->e_shnum;
	off = ((Elf64_Ehdr*)elf->e_ehdr)->e_shoff;
	/*
	 * Check for overflow on 32-bit systems
	 */
	if (overflow(off, ((Elf64_Ehdr*)elf->e_ehdr)->e_shoff, Elf64_Off)) {
	    seterr(ERROR_OUTSIDE);
	    return 0;
	}
    }
#endif /* __LIBELF64 */
    /* we already had this
    else {
	seterr(ERROR_UNIMPLEMENTED);
	return 0;
    }
    */
    if (num && off) {
	struct tmp {
	    Elf_Scn	scn;
	    Scn_Data	data;
	} *head;
	Elf_Data src, dst;
	Elf_Scn *scn;
	Scn_Data *sd;
	unsigned i;

	if (off % align_addr) {
	    seterr(ERROR_ALIGN_SHDR);
	    return 0;
	}
	if (off < 0 || off > elf->e_size) {
	    seterr(ERROR_OUTSIDE);
	    return 0;
	}

	src.d_type = ELF_T_SHDR;
	src.d_version = elf->e_version;
	src.d_size = _fsize(elf->e_class, src.d_version, ELF_T_SHDR);
	elf_assert(src.d_size);
	dst.d_version = EV_CURRENT;

	if (off + num * src.d_size < off /* modulo overflow */
	 || off + num * src.d_size > elf->e_size) {
	    seterr(ERROR_TRUNC_SHDR);
	    return 0;
	}

	if (!(head = (struct tmp*)malloc(num * sizeof(struct tmp)))) {
	    seterr(ERROR_MEM_SCN);
	    return 0;
	}
	for (scn = NULL, i = num; i-- > 0; ) {
	    head[i].scn = _elf_scn_init;
	    head[i].data = _elf_data_init;
	    head[i].scn.s_link = scn;
	    if (!scn) {
		elf->e_scn_n = &head[i].scn;
	    }
	    scn = &head[i].scn;
	    sd = &head[i].data;

	    if (elf->e_rawdata) {
		src.d_buf = elf->e_rawdata + off + i * src.d_size;
	    }
	    else {
		src.d_buf = elf->e_data + off + i * src.d_size;
	    }
	    dst.d_buf = &scn->s_uhdr;
	    dst.d_size = sizeof(scn->s_uhdr);
	    if (!(_elf_xlatetom(elf, &dst, &src))) {
		elf->e_scn_n = NULL;
		free(head);
		return 0;
	    }
	    elf_assert(dst.d_size == _msize(elf->e_class, EV_CURRENT, ELF_T_SHDR));
	    elf_assert(dst.d_type == ELF_T_SHDR);

	    scn->s_elf = elf;
	    scn->s_index = i;
	    scn->s_data_1 = sd;
	    scn->s_data_n = sd;

	    sd->sd_scn = scn;

	    if (elf->e_class == ELFCLASS32) {
		Elf32_Shdr *shdr = &scn->s_shdr32;

		scn->s_type = shdr->sh_type;
		scn->s_size = shdr->sh_size;
		scn->s_offset = shdr->sh_offset;
		sd->sd_data.d_align = shdr->sh_addralign;
	    }
#if __LIBELF64
	    else if (elf->e_class == ELFCLASS64) {
		Elf64_Shdr *shdr = &scn->s_shdr64;

		scn->s_type = shdr->sh_type;
		scn->s_size = shdr->sh_size;
		scn->s_offset = shdr->sh_offset;
		sd->sd_data.d_align = shdr->sh_addralign;
		/*
		 * Check for overflow on 32-bit systems
		 */
		if (overflow(scn->s_size, shdr->sh_size, Elf64_Xword)
		 || overflow(scn->s_offset, shdr->sh_offset, Elf64_Off)
		 || overflow(sd->sd_data.d_align, shdr->sh_addralign, Elf64_Xword)) {
		    seterr(ERROR_OUTSIDE);
		    return 0;
		}
	    }
#endif /* __LIBELF64 */
	    /* we already had this
	    else {
		seterr(ERROR_UNIMPLEMENTED);
		return 0;
	    }
	    */

	    sd->sd_data.d_type = _elf_scn_type(scn->s_type);
	    sd->sd_data.d_size = scn->s_size;
	    sd->sd_data.d_version = _elf_version;
	}
	elf_assert(scn == &head[0].scn);
	elf->e_scn_1 = &head[0].scn;
	head[0].scn.s_freeme = 1;
    }
    return 1;
}
/*@=uniondef@*/

int
_elf_cook(Elf *elf) {
    elf_assert(_elf_scn_init.s_magic == SCN_MAGIC);
    elf_assert(_elf_data_init.sd_magic == DATA_MAGIC);
    elf_assert(elf);
    elf_assert(elf->e_magic == ELF_MAGIC);
    elf_assert(elf->e_kind == ELF_K_ELF);
    elf_assert(!elf->e_ehdr);
    if (!valid_version(elf->e_version)) {
	seterr(ERROR_UNKNOWN_VERSION);
    }
    else if (!valid_encoding(elf->e_encoding)) {
	seterr(ERROR_UNKNOWN_ENCODING);
    }
    else if (valid_class(elf->e_class)) {
	return _elf_cook_file(elf);
    }
    else {
	seterr(ERROR_UNKNOWN_CLASS);
    }
    return 0;
}
