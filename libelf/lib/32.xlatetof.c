/*
32.xlatetof.c - implementation of the elf32_xlateto[fm](3) functions.
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

#include <private.h>
#include <ext_types.h>
#include <byteswap.h>

/*
 * Ugly, ugly
 */
#define x
#if defined/**/x
# define Cat2(a,b)a##b
# define Cat3(a,b,c)a##b##c
# define Exn(m1,m2,args)m1##m2##args
# define Ex1(m1,m2,a,b)m1##m2(a##b)
# define Ex2(m1,m2,a,b,c)m1##m2(a,b##c)
#else
# define Cat2(a,b)a/**/b
# define Cat3(a,b,c)a/**/b/**/c
# define Exn(m1,m2,args)m1/**/m2/**/args
# define Ex1(m1,m2,a,b)m1/**/m2(a/**/b)
# define Ex2(m1,m2,a,b,c)m1/**/m2(a,b/**/c)
#endif
#undef x

/*
 * auxiliary macros for execution order reversal
 */
#define seq_forw(a,b) a b
#define seq_back(a,b) b a

/*
 * function instantiator
 */
#define copy_type_e_io(name,e,io,tfrom,tto,copy)		\
    static void							\
    Cat3(name,_,io)(unsigned char *dst, const unsigned char *src, size_t n) {	\
	const tfrom *from = (const tfrom*)src;			\
	tto *to = (tto*)dst;					\
	if (sizeof(tfrom) < sizeof(tto)) {			\
	    from += n;						\
	    to += n;						\
	    while (n-- > 0) {					\
		--from;						\
		--to;						\
		copy(e,io,seq_back)				\
	    }							\
	}							\
	else {							\
	    while (n-- > 0) {					\
		copy(e,io,seq_forw)				\
		from++;						\
		to++;						\
	    }							\
	}							\
    }

#define copy_type_e(name,e,type,copy)				\
    copy_type_e_io(name,e,tom,Cat2(__ext_,type),type,copy)	\
    copy_type_e_io(name,e,tof,type,Cat2(__ext_,type),copy)

/*
 * master function instantiator
 */
#define copy_type(name,version,type,copy)		\
    copy_type_e(Cat3(name,L,version),L,type,copy)	\
    copy_type_e(Cat3(name,M,version),M,type,copy)

/*
 * scalar copying
 */
#define copy_scalar_tom(type)	*to = Exn(__load_,type,(*from));
#define copy_scalar_tof(type)	Exn(__store_,type,(*to, *from));

/*
 * structure member copying
 */
#define copy_tom(mb,type)	to->mb = Exn(__load_,type,(from->mb));
#define copy_tof(mb,type)	Exn(__store_,type,(to->mb, from->mb));

/*
 * structure member copying (direction independent)
 */
#define copy_byte(e,io,mb)	to->mb = from->mb;
#define copy_addr(e,io,mb)	Ex2(copy_,io,mb,u32,e)
#define copy_half(e,io,mb)	Ex2(copy_,io,mb,u16,e)
#define copy_off(e,io,mb)	Ex2(copy_,io,mb,u32,e)
#define copy_sword(e,io,mb)	Ex2(copy_,io,mb,i32,e)
#define copy_word(e,io,mb)	Ex2(copy_,io,mb,u32,e)
#define copy_arr(e,io,mb)	\
    array_copy(to->mb, sizeof(to->mb), from->mb, sizeof(from->mb));

/*
 * scalar copying (direction independent)
 * these macros are used as `copy' arguments to copy_type()
 */
#define copy_addr_11(e,io,seq)	Ex1(copy_scalar_,io,u32,e)
#define copy_half_11(e,io,seq)	Ex1(copy_scalar_,io,u16,e)
#define copy_off_11(e,io,seq)	Ex1(copy_scalar_,io,u32,e)
#define copy_sword_11(e,io,seq)	Ex1(copy_scalar_,io,i32,e)
#define copy_word_11(e,io,seq)	Ex1(copy_scalar_,io,u32,e)

/*
 * structure copying (direction independent)
 * these macros are used as `copy' arguments to copy_type()
 */
#define copy_dyn_11(e,io,seq)		\
    seq(copy_sword(e,io,d_tag),		\
    seq(copy_addr(e,io,d_un.d_ptr),	\
    /**/))
#define copy_ehdr_11(e,io,seq)		\
    seq(copy_arr(e,io,e_ident),		\
    seq(copy_half(e,io,e_type),		\
    seq(copy_half(e,io,e_machine),	\
    seq(copy_word(e,io,e_version),	\
    seq(copy_addr(e,io,e_entry),	\
    seq(copy_off(e,io,e_phoff),		\
    seq(copy_off(e,io,e_shoff),		\
    seq(copy_word(e,io,e_flags),	\
    seq(copy_half(e,io,e_ehsize),	\
    seq(copy_half(e,io,e_phentsize),	\
    seq(copy_half(e,io,e_phnum),	\
    seq(copy_half(e,io,e_shentsize),	\
    seq(copy_half(e,io,e_shnum),	\
    seq(copy_half(e,io,e_shstrndx),	\
    /**/))))))))))))))
#define copy_phdr_11(e,io,seq)		\
    seq(copy_word(e,io,p_type),		\
    seq(copy_off(e,io,p_offset),	\
    seq(copy_addr(e,io,p_vaddr),	\
    seq(copy_addr(e,io,p_paddr),	\
    seq(copy_word(e,io,p_filesz),	\
    seq(copy_word(e,io,p_memsz),	\
    seq(copy_word(e,io,p_flags),	\
    seq(copy_word(e,io,p_align),	\
    /**/))))))))
#define copy_rela_11(e,io,seq)		\
    seq(copy_addr(e,io,r_offset),	\
    seq(copy_word(e,io,r_info),		\
    seq(copy_sword(e,io,r_addend),	\
    /**/)))
#define copy_rel_11(e,io,seq)		\
    seq(copy_addr(e,io,r_offset),	\
    seq(copy_word(e,io,r_info),		\
    /**/))
#define copy_shdr_11(e,io,seq)		\
    seq(copy_word(e,io,sh_name),	\
    seq(copy_word(e,io,sh_type),	\
    seq(copy_word(e,io,sh_flags),	\
    seq(copy_addr(e,io,sh_addr),	\
    seq(copy_off(e,io,sh_offset),	\
    seq(copy_word(e,io,sh_size),	\
    seq(copy_word(e,io,sh_link),	\
    seq(copy_word(e,io,sh_info),	\
    seq(copy_word(e,io,sh_addralign),	\
    seq(copy_word(e,io,sh_entsize),	\
    /**/))))))))))
#define copy_sym_11(e,io,seq)		\
    seq(copy_word(e,io,st_name),	\
    seq(copy_addr(e,io,st_value),	\
    seq(copy_word(e,io,st_size),	\
    seq(copy_byte(e,io,st_info),	\
    seq(copy_byte(e,io,st_other),	\
    seq(copy_half(e,io,st_shndx),	\
    /**/))))))

static void
byte_copy(unsigned char *dst, const unsigned char *src, size_t n) {
    if (dst == src || !n) {
	return;
    }
#if HAVE_BROKEN_MEMMOVE
    while (dst > src && dst < &src[n]) {
	if (n <= 16) {
	    /* copy `manually' */
	    while (n--) {
		dst[n] = src[n];
	    }
	    return;
	}
	/* copy upper half */
	byte_copy(&dst[n / 2], &src[n / 2], n - n / 2);
	/* continue with lower half */
	n /= 2;
    }
#endif
    memmove(dst, src, n);
}

static void
array_copy(unsigned char *dst, size_t dlen, const unsigned char *src, size_t slen) {
    byte_copy(dst, src, dlen < slen ? dlen : slen);
    if (dlen > slen) {
	memset(dst + slen, 0, dlen - slen);
    }
}

/*
 * instantiate copy functions
 */
copy_type(addr_32,,Elf32_Addr,copy_addr_11)
copy_type(half_32,,Elf32_Half,copy_half_11)
copy_type(off_32,,Elf32_Off,copy_off_11)
copy_type(sword_32,,Elf32_Sword,copy_sword_11)
copy_type(word_32,,Elf32_Word,copy_word_11)
copy_type(dyn_32,11,Elf32_Dyn,copy_dyn_11)
copy_type(ehdr_32,11,Elf32_Ehdr,copy_ehdr_11)
copy_type(phdr_32,11,Elf32_Phdr,copy_phdr_11)
copy_type(rela_32,11,Elf32_Rela,copy_rela_11)
copy_type(rel_32,11,Elf32_Rel,copy_rel_11)
copy_type(shdr_32,11,Elf32_Shdr,copy_shdr_11)
copy_type(sym_32,11,Elf32_Sym,copy_sym_11)

typedef void (*xlator)(unsigned char*, const unsigned char*, size_t);
typedef xlator xltab[ELF_T_NUM][2];

/*
 * translation table (32-bit, version 1 -> version 1)
 */
static const xltab
xlate32_11[/*encoding*/] = {
    {
	{ byte_copy,	    byte_copy	    },
	{ addr_32L_tom,	    addr_32L_tof    },
	{ dyn_32L11_tom,    dyn_32L11_tof   },
	{ ehdr_32L11_tom,   ehdr_32L11_tof  },
	{ half_32L_tom,	    half_32L_tof    },
	{ off_32L_tom,	    off_32L_tof	    },
	{ phdr_32L11_tom,   phdr_32L11_tof  },
	{ rela_32L11_tom,   rela_32L11_tof  },
	{ rel_32L11_tom,    rel_32L11_tof   },
	{ shdr_32L11_tom,   shdr_32L11_tof  },
	{ sword_32L_tom,    sword_32L_tof   },
	{ sym_32L11_tom,    sym_32L11_tof   },
	{ word_32L_tom,	    word_32L_tof    },
    },
    {
	{ byte_copy,	    byte_copy	    },
	{ addr_32M_tom,	    addr_32M_tof    },
	{ dyn_32M11_tom,    dyn_32M11_tof   },
	{ ehdr_32M11_tom,   ehdr_32M11_tof  },
	{ half_32M_tom,	    half_32M_tof    },
	{ off_32M_tom,	    off_32M_tof	    },
	{ phdr_32M11_tom,   phdr_32M11_tof  },
	{ rela_32M11_tom,   rela_32M11_tof  },
	{ rel_32M11_tom,    rel_32M11_tof   },
	{ shdr_32M11_tom,   shdr_32M11_tof  },
	{ sword_32M_tom,    sword_32M_tof   },
	{ sym_32M11_tom,    sym_32M11_tof   },
	{ word_32M_tom,	    word_32M_tof    },
    },
};

/*
 * main translation table (32-bit)
 */
static const xltab *const
xlate32[EV_CURRENT - EV_NONE][EV_CURRENT - EV_NONE] = {
    { xlate32_11, },
};

#define translator(sv,dv,enc,type,d)	\
    (xlate32[(sv) - EV_NONE - 1]	\
	    [(dv) - EV_NONE - 1]	\
	    [(enc) - ELFDATA2LSB]	\
	    [(type) - ELF_T_BYTE]	\
	    [d])

/*
 * direction-independent translation
 */
static Elf_Data*
elf32_xlate(Elf_Data *dst, const Elf_Data *src, unsigned encode, int tof) {
    size_t ssize, dsize, count;
    Elf_Type type;
    int sv, dv;
    xlator op;

    if (!src || !dst) {
	return NULL;
    }
    if (!src->d_buf || !dst->d_buf) {
	seterr(ERROR_NULLBUF);
	return NULL;
    }
    if (!valid_encoding(encode)) {
	seterr(ERROR_UNKNOWN_ENCODING);
	return NULL;
    }
    sv = src->d_version;
    dv = dst->d_version;
    if (!valid_version(sv) || !valid_version(dv)) {
	seterr(ERROR_UNKNOWN_VERSION);
	return NULL;
    }
    type = src->d_type;
    if (!valid_type(type)) {
	seterr(ERROR_UNKNOWN_TYPE);
	return NULL;
    }
    ssize = _fmsize32(sv, type, 1 - tof);
    dsize = _fmsize32(dv, type, tof);
    op = translator(sv, dv, encode, type, tof);
    if (!ssize || !dsize || !op) {
	seterr(ERROR_UNKNOWN_TYPE);
	return NULL;
    }
    count = src->d_size / ssize;
    if (dst->d_size < count * dsize) {
	seterr(ERROR_DST2SMALL);
	return NULL;
    }
    if (count) {
	(*op)(dst->d_buf, src->d_buf, count);
    }
    dst->d_size = count * dsize;
    dst->d_type = type;
    return dst;
}

/*
 * finally, the "official" translation functions
 */
Elf_Data*
elf32_xlatetom(Elf_Data *dst, const Elf_Data *src, unsigned encode) {
    return elf32_xlate(dst, src, encode, 0);
}

Elf_Data*
elf32_xlatetof(Elf_Data *dst, const Elf_Data *src, unsigned encode) {
    return elf32_xlate(dst, src, encode, 1);
}
