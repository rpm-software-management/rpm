/*
verneed.h - copy versioning information.
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
static const char verneed_h_rcsid[] = "@(#) Id: verneed.h,v 1.1 2001/10/07 20:03:02 michael Exp ";
#endif /* lint */

#if VER_NEED_CURRENT != 1
#error libelf currently does not support VER_NEED_CURRENT != 1
#endif /* VER_NEED_CURRENT != 1 */

#if TOFILE

static void
__store_vernaux(vernaux_ftype *dst, const vernaux_mtype *src, unsigned enc)
	/*@modifies *dst @*/
{
    if (enc == ELFDATA2LSB) {
	__store_u32L(dst->vna_hash,  src->vna_hash);
	__store_u16L(dst->vna_flags, src->vna_flags);
	__store_u16L(dst->vna_other, src->vna_other);
	__store_u32L(dst->vna_name,  src->vna_name);
	__store_u32L(dst->vna_next,  src->vna_next);
    }
    else {
	__store_u32M(dst->vna_hash,  src->vna_hash);
	__store_u16M(dst->vna_flags, src->vna_flags);
	__store_u16M(dst->vna_other, src->vna_other);
	__store_u32M(dst->vna_name,  src->vna_name);
	__store_u32M(dst->vna_next,  src->vna_next);
    }
}

static void
__store_verneed(verneed_ftype *dst, const verneed_mtype *src, unsigned enc)
	/*@modifies *dst @*/
{
    if (enc == ELFDATA2LSB) {
	__store_u16L(dst->vn_version, src->vn_version);
	__store_u16L(dst->vn_cnt,     src->vn_cnt);
	__store_u32L(dst->vn_file,    src->vn_file);
	__store_u32L(dst->vn_aux,     src->vn_aux);
	__store_u32L(dst->vn_next,    src->vn_next);
    }
    else {
	__store_u16M(dst->vn_version, src->vn_version);
	__store_u16M(dst->vn_cnt,     src->vn_cnt);
	__store_u32M(dst->vn_file,    src->vn_file);
	__store_u32M(dst->vn_aux,     src->vn_aux);
	__store_u32M(dst->vn_next,    src->vn_next);
    }
}

typedef vernaux_mtype		vernaux_stype;
typedef vernaux_ftype		vernaux_dtype;
typedef verneed_mtype		verneed_stype;
typedef verneed_ftype		verneed_dtype;
typedef align_mtype		verneed_atype;

#define copy_vernaux_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_vernaux_tmptodst(d, s, e)	__store_vernaux((d), (s), (e))
#define copy_verneed_srctotmp(d, s, e)	(*(d) = *(s))
#define copy_verneed_tmptodst(d, s, e)	__store_verneed((d), (s), (e))

#define translator_suffix	11_tof

#else /* TOFILE */

static void
__load_vernaux(vernaux_mtype *dst, const vernaux_ftype *src, unsigned enc)
	/*@modifies *dst @*/
{
/*@-boundsread@*/
    if (enc == ELFDATA2LSB) {
	dst->vna_hash  = __load_u32L(src->vna_hash);
	dst->vna_flags = __load_u16L(src->vna_flags);
	dst->vna_other = __load_u16L(src->vna_other);
	dst->vna_name  = __load_u32L(src->vna_name);
	dst->vna_next  = __load_u32L(src->vna_next);
    }
    else {
	dst->vna_hash  = __load_u32M(src->vna_hash);
	dst->vna_flags = __load_u16M(src->vna_flags);
	dst->vna_other = __load_u16M(src->vna_other);
	dst->vna_name  = __load_u32M(src->vna_name);
	dst->vna_next  = __load_u32M(src->vna_next);
    }
/*@=boundsread@*/
}

static void
__load_verneed(verneed_mtype *dst, const verneed_ftype *src, unsigned enc)
	/*@modifies *dst @*/
{
/*@-boundsread@*/
    if (enc == ELFDATA2LSB) {
	dst->vn_version = __load_u16L(src->vn_version);
	dst->vn_cnt     = __load_u16L(src->vn_cnt);
	dst->vn_file    = __load_u32L(src->vn_file);
	dst->vn_aux     = __load_u32L(src->vn_aux);
	dst->vn_next    = __load_u32L(src->vn_next);
    }
    else {
	dst->vn_version = __load_u16M(src->vn_version);
	dst->vn_cnt     = __load_u16M(src->vn_cnt);
	dst->vn_file    = __load_u32M(src->vn_file);
	dst->vn_aux     = __load_u32M(src->vn_aux);
	dst->vn_next    = __load_u32M(src->vn_next);
    }
/*@=boundsread@*/
}

typedef vernaux_ftype		vernaux_stype;
typedef vernaux_mtype		vernaux_dtype;
typedef verneed_ftype		verneed_stype;
typedef verneed_mtype		verneed_dtype;
typedef align_ftype		verneed_atype;

#define copy_vernaux_srctotmp(d, s, e)	__load_vernaux((d), (s), (e))
#define copy_vernaux_tmptodst(d, s, e)	(*(d) = *(s))
#define copy_verneed_srctotmp(d, s, e)	__load_verneed((d), (s), (e))
#define copy_verneed_tmptodst(d, s, e)	(*(d) = *(s))

#define translator_suffix	11_tom

#endif /* TOFILE */

#define cat3(a,b,c)	a##b##c
#define xlt3(p,e,s)	cat3(p,e,s)
#define xltprefix(x)	xlt3(x,_,class_suffix)
#define translator(x,e)	xlt3(xltprefix(_elf_##x),e,translator_suffix)

static size_t
xlt_verneed(unsigned char *dst, const unsigned char *src, size_t n, unsigned enc)
	/*@globals _elf_errno @*/
	/*@modifies *dst, _elf_errno @*/
{
    size_t doff;
    size_t soff;

    if (n < sizeof(verneed_stype)) {
	return 0;
    }
    soff = doff = 0;
    for (;;) {
	const verneed_stype *svn;
	verneed_dtype *dvn;
	verneed_mtype vn;
	size_t acount;
	size_t aoff;
	size_t save = doff;

	/*
	 * allocate space in dst buffer
	 */
	dvn = (verneed_dtype*)(dst + doff);
	doff += sizeof(verneed_dtype);
	/*
	 * load and check src
	 */
	svn = (verneed_stype*)(src + soff);
	copy_verneed_srctotmp(&vn, svn, enc);
	if (vn.vn_version < 1
	 || vn.vn_version > VER_NEED_CURRENT) {
	    seterr(ERROR_VERNEED_VERSION);
	    return (size_t)-1;
	}
	if (vn.vn_cnt < 1
	 || vn.vn_aux == 0
	 || vn.vn_aux % sizeof(verneed_atype)
	 || vn.vn_aux < sizeof(verneed_stype)) {
	    seterr(ERROR_VERNEED_FORMAT);
	    return (size_t)-1;
	}
	/*
	 * get Vernaux offset and advance to next Verneed
	 */
	aoff = soff + vn.vn_aux;
	if (vn.vn_next != 0) {
	    if (vn.vn_next % sizeof(verneed_atype)
	     || vn.vn_next < sizeof(verneed_stype)) {
		seterr(ERROR_VERNEED_FORMAT);
		return (size_t)-1;
	    }
	    soff += vn.vn_next;
	    if (soff + sizeof(verneed_stype) > n) {
		seterr(ERROR_VERNEED_FORMAT);
		return (size_t)-1;
	    }
	}
	/*
	 * read Vernaux array
	 */
	for (acount = 1; ; acount++) {
	    const vernaux_stype *svna;
	    vernaux_dtype *dvna;
	    vernaux_mtype vna;

	    /*
	     * check for src buffer overflow
	     */
	    if (aoff + sizeof(vernaux_stype) > n) {
		seterr(ERROR_VERNEED_FORMAT);
		return (size_t)-1;
	    }
	    /*
	     * allocate space in dst buffer
	     */
	    dvna = (vernaux_dtype*)(dst + doff);
	    doff += sizeof(vernaux_dtype);
	    /*
	     * load and check src
	     */
	    svna = (vernaux_stype*)(src + aoff);
	    copy_vernaux_srctotmp(&vna, svna, enc);
	    if (vna.vna_next != 0) {
		if (vna.vna_next % sizeof(verneed_atype)
		 || vna.vna_next < sizeof(vernaux_stype)) {
		    seterr(ERROR_VERNEED_FORMAT);
		    return (size_t)-1;
		}
		aoff += vna.vna_next;
		vna.vna_next = sizeof(vernaux_dtype);
	    }
	    /*
	     * copy Vernaux to dst buffer
	     */
	    if (dst) {
		copy_vernaux_tmptodst(dvna, &vna, enc);
	    }
	    /*
	     * end check
	     */
	    if (vna.vna_next == 0) {
		/*@innerbreak@*/ break;
	    }
	}
	/*
	 * parameter check
	 */
	if (acount != vn.vn_cnt) {
	    seterr(ERROR_VERNEED_FORMAT);
	    return (size_t)-1;
	}
	/*
	 * copy Verneed to dst buffer
	 */
	if (dst) {
	    vn.vn_aux = sizeof(verneed_dtype);
	    if (vn.vn_next != 0) {
		vn.vn_next = doff - save;
	    }
	    copy_verneed_tmptodst(dvn, &vn, enc);
	}
	/*
	 * end check
	 */
	if (vn.vn_next == 0) {
	    return doff;
	}
    }
}

size_t
translator(verneed,L)(unsigned char *dst, const unsigned char *src, size_t n)
	/*@modifies *dst @*/
{
    return xlt_verneed(dst, src, n, ELFDATA2LSB);
}

size_t
translator(verneed,M)(unsigned char *dst, const unsigned char *src, size_t n)
	/*@modifies *dst @*/
{
    return xlt_verneed(dst, src, n, ELFDATA2MSB);
}
