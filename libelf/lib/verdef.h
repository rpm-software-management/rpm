/*
verdef.h - copy versioning information.
Copyright (C) 2001 Michael Riepe <michael@stud.uni-hannover.de>

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

#ifndef lint
static const char verdef_h_rcsid[] = "@(#) Id: verdef.h,v 1.1 2001/10/07 20:03:02 michael Exp ";
#endif /* lint */

#if VER_DEF_CURRENT != 1
#error libelf currently does not support VER_DEF_CURRENT != 1
#endif /* VER_DEF_CURRENT != 1 */

#if TOFILE

static void
__store_verdaux(verdaux_ftype *dst, const verdaux_mtype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	__store_u32L(dst->vda_name, src->vda_name);
	__store_u32L(dst->vda_next, src->vda_next);
    }
    else {
	__store_u32M(dst->vda_name, src->vda_name);
	__store_u32M(dst->vda_next, src->vda_next);
    }
}

static void
__store_verdef(verdef_ftype *dst, const verdef_mtype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	__store_u16L(dst->vd_version, src->vd_version);
	__store_u16L(dst->vd_flags,   src->vd_flags);
	__store_u16L(dst->vd_ndx,     src->vd_ndx);
	__store_u16L(dst->vd_cnt,     src->vd_cnt);
	__store_u32L(dst->vd_hash,    src->vd_hash);
	__store_u32L(dst->vd_aux,     src->vd_aux);
	__store_u32L(dst->vd_next,    src->vd_next);
    }
    else {
	__store_u16M(dst->vd_version, src->vd_version);
	__store_u16M(dst->vd_flags,   src->vd_flags);
	__store_u16M(dst->vd_ndx,     src->vd_ndx);
	__store_u16M(dst->vd_cnt,     src->vd_cnt);
	__store_u32M(dst->vd_hash,    src->vd_hash);
	__store_u32M(dst->vd_aux,     src->vd_aux);
	__store_u32M(dst->vd_next,    src->vd_next);
    }
}

typedef verdaux_mtype		verdaux_stype;
typedef verdaux_ftype		verdaux_dtype;
typedef verdef_mtype		verdef_stype;
typedef verdef_ftype		verdef_dtype;
typedef align_mtype		verdef_atype;

#define copy_verdaux_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_verdaux_tmptodst(d, s, e)	__store_verdaux((d), (s), (e))
#define copy_verdef_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_verdef_tmptodst(d, s, e)	__store_verdef((d), (s), (e))

#define translator_suffix	11_tof

#else /* TOFILE */

static void
__load_verdaux(verdaux_mtype *dst, const verdaux_ftype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	dst->vda_name = __load_u32L(src->vda_name);
	dst->vda_next = __load_u32L(src->vda_next);
    }
    else {
	dst->vda_name = __load_u32M(src->vda_name);
	dst->vda_next = __load_u32M(src->vda_next);
    }
}

static void
__load_verdef(verdef_mtype *dst, const verdef_ftype *src, unsigned enc) {
    if (enc == ELFDATA2LSB) {
	dst->vd_version = __load_u16L(src->vd_version);
	dst->vd_flags   = __load_u16L(src->vd_flags);
	dst->vd_ndx     = __load_u16L(src->vd_ndx);
	dst->vd_cnt     = __load_u16L(src->vd_cnt);
	dst->vd_hash    = __load_u32L(src->vd_hash);
	dst->vd_aux     = __load_u32L(src->vd_aux);
	dst->vd_next    = __load_u32L(src->vd_next);
    }
    else {
	dst->vd_version = __load_u16M(src->vd_version);
	dst->vd_flags   = __load_u16M(src->vd_flags);
	dst->vd_ndx     = __load_u16M(src->vd_ndx);
	dst->vd_cnt     = __load_u16M(src->vd_cnt);
	dst->vd_hash    = __load_u32M(src->vd_hash);
	dst->vd_aux     = __load_u32M(src->vd_aux);
	dst->vd_next    = __load_u32M(src->vd_next);
    }
}

typedef verdaux_ftype		verdaux_stype;
typedef verdaux_mtype		verdaux_dtype;
typedef verdef_ftype		verdef_stype;
typedef verdef_mtype		verdef_dtype;
typedef align_ftype		verdef_atype;

#define copy_verdaux_srctotmp(d, s, e)	__load_verdaux((d), (s), (e))
#define copy_verdaux_tmptodst(d, s, e)	(*(d) = *(s))
#define copy_verdef_srctotmp(d, s, e)	__load_verdef((d), (s), (e))
#define copy_verdef_tmptodst(d, s, e)	(*(d) = *(s))

#define translator_suffix	11_tom

#endif /* TOFILE */

#define cat3(a,b,c)	a##b##c
#define xlt3(p,e,s)	cat3(p,e,s)
#define xltprefix(x)	xlt3(x,_,class_suffix)
#define translator(x,e)	xlt3(xltprefix(_elf_##x),e,translator_suffix)

static size_t
xlt_verdef(unsigned char *dst, const unsigned char *src, size_t n, unsigned enc) {
    size_t doff;
    size_t soff;

    if (n < sizeof(verdef_stype)) {
	return 0;
    }
    soff = doff = 0;
    for (;;) {
	const verdef_stype *svd;
	verdef_dtype *dvd;
	verdef_mtype vd;
	size_t acount;
	size_t aoff;
	size_t save = doff;

	/*
	 * allocate space in dst buffer
	 */
	dvd = (verdef_dtype*)(dst + doff);
	doff += sizeof(verdef_dtype);
	/*
	 * load and check src
	 */
	svd = (verdef_stype*)(src + soff);
	copy_verdef_srctotmp(&vd, svd, enc);
	if (vd.vd_version < 1
	 || vd.vd_version > VER_DEF_CURRENT) {
	    seterr(ERROR_VERDEF_VERSION);
	    return (size_t)-1;
	}
	if (vd.vd_cnt < 1
	 || vd.vd_aux == 0
	 || vd.vd_aux % sizeof(verdef_atype)
	 || vd.vd_aux < sizeof(verdef_stype)) {
	    seterr(ERROR_VERDEF_FORMAT);
	    return (size_t)-1;
	}
	/*
	 * get Verdaux offset and advance to next Verdef
	 */
	aoff = soff + vd.vd_aux;
	if (vd.vd_next != 0) {
	    if (vd.vd_next % sizeof(verdef_atype)
	     || vd.vd_next < sizeof(verdef_stype)) {
		seterr(ERROR_VERDEF_FORMAT);
		return (size_t)-1;
	    }
	    soff += vd.vd_next;
	    if (soff + sizeof(verdef_stype) > n) {
		seterr(ERROR_VERDEF_FORMAT);
		return (size_t)-1;
	    }
	}
	/*
	 * read Verdaux array
	 */
	for (acount = 1; ; acount++) {
	    const verdaux_stype *svda;
	    verdaux_dtype *dvda;
	    verdaux_mtype vda;

	    /*
	     * check for src buffer overflow
	     */
	    if (aoff + sizeof(verdaux_stype) > n) {
		seterr(ERROR_VERDEF_FORMAT);
		return (size_t)-1;
	    }
	    /*
	     * allocate space in dst buffer
	     */
	    dvda = (verdaux_dtype*)(dst + doff);
	    doff += sizeof(verdaux_dtype);
	    /*
	     * load and check src
	     */
	    svda = (verdaux_stype*)(src + aoff);
	    copy_verdaux_srctotmp(&vda, svda, enc);
	    if (vda.vda_next != 0) {
		if (vda.vda_next % sizeof(verdef_atype)
		 || vda.vda_next < sizeof(verdaux_stype)) {
		    seterr(ERROR_VERDEF_FORMAT);
		    return (size_t)-1;
		}
		aoff += vda.vda_next;
		vda.vda_next = sizeof(verdaux_dtype);
	    }
	    /*
	     * copy Verdaux to dst buffer
	     */
	    if (dst) {
		copy_verdaux_tmptodst(dvda, &vda, enc);
	    }
	    /*
	     * end check
	     */
	    if (vda.vda_next == 0) {
		break;
	    }
	}
	/*
	 * parameter check
	 */
	if (acount != vd.vd_cnt) {
	    seterr(ERROR_VERDEF_FORMAT);
	    return (size_t)-1;
	}
	/*
	 * copy Verdef to dst buffer
	 */
	if (dst) {
	    vd.vd_aux = sizeof(verdef_dtype);
	    if (vd.vd_next != 0) {
		vd.vd_next = doff - save;
	    }
	    copy_verdef_tmptodst(dvd, &vd, enc);
	}
	/*
	 * end check
	 */
	if (vd.vd_next == 0) {
	    return doff;
	}
    }
}

size_t
translator(verdef,L)(unsigned char *dst, const unsigned char *src, size_t n) {
    return xlt_verdef(dst, src, n, ELFDATA2LSB);
}

size_t
translator(verdef,M)(unsigned char *dst, const unsigned char *src, size_t n) {
    return xlt_verdef(dst, src, n, ELFDATA2MSB);
}
