/** \ingroup DSA_m
 * \file dsa.c
 *
 * Digital Signature Algorithm signature scheme, code.
 *
 * DSA Signature:
 *  - Signing equation:
 *   - r = (g^k mod p) mod q and
 *   - s = (inv(k) * (h(m) + x*r)) mod q
 *  - Verifying equation:
 *   - check 0 < r < q and 0 < s < q
 *   - w = inv(s) mod q
 *   - u1 = (h(m)*w) mod q
 *   - u2 = (r*w) mod q
 *   - v = ((g^u1 * y^u2) mod p) mod q
 *   - check v == r
 *
 * For more information on this algorithm, see:
 *  NIST FIPS 186-1
 */

/*
 * Copyright (c) 2001 Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */
 
#define BEECRYPT_DLL_EXPORT

#include "dsa.h"
#include "dldp.h"
#include "mp32.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

/*@unchecked@*/
static int _debug = 0;

int dsasign(const mp32barrett* p, const mp32barrett* q, const mp32number* g, randomGeneratorContext* rgc, const mp32number* hm, const mp32number* x, mp32number* r, mp32number* s)
{
	register uint32  psize = p->size;
	register uint32  qsize = q->size;
	// k + inv(k) = 2 * qsize
	// g^k mod p = psize+4*psize+2

	register uint32* ptemp;
	register uint32* qtemp;
	register int rc = -1;	/* assume failure */

	ptemp = (uint32*) malloc((5*psize+2) * sizeof(*ptemp));
	if (ptemp == NULL)
		return rc;
	qtemp = (uint32*) malloc((9*qsize+6) * sizeof(*qtemp));
	if (qtemp == NULL) {
		free(ptemp);
		return rc;
	}

	{
		register uint32* pwksp = ptemp+psize;
		register uint32* qwksp = qtemp+3*qsize;

		// allocate r
		mp32nfree(r);
		mp32nsize(r, qsize);

		// get a random k, invertible modulo q
		mp32brndinv_w(q, rgc, qtemp, qtemp+qsize, qwksp);

#if 0
/* FIPS 186 test vectors for k, http://www.itl.nist.gov/fipspubs/186chg-1.htm */
		qtemp[0] = 0x358dad57;
		qtemp[1] = 0x1462710f;
		qtemp[2] = 0x50e254cf;
		qtemp[3] = 0x1a376b2b;
		qtemp[4] = 0xdeaadfbf;

		mp32binv_w(q, qsize, qtemp, qtemp+qsize, qwksp);
#endif

		// g^k mod p
		mp32bpowmod_w(p, g->size, g->data, qsize, qtemp, ptemp, pwksp);

		// (g^k mod p) mod q - simple modulo
		mp32nmod(qtemp+2*qsize, psize, ptemp, qsize, q->modl, pwksp);
		mp32copy(qsize, r->data, qtemp+psize+qsize);

		// allocate s
		mp32nfree(s);
		mp32nsize(s, qsize);

		// x*r mod q
		mp32bmulmod_w(q, x->size, x->data, r->size, r->data, qtemp, qwksp);

		// add h(m) mod q
		mp32baddmod_w(q, qsize, qtemp, hm->size, hm->data, qtemp+2*qsize, qwksp);

		// multiply inv(k) mod q
		mp32bmulmod_w(q, qsize, qtemp+qsize, qsize, qtemp+2*qsize, s->data, qwksp);
		rc = 0;

	}
	free(qtemp);
	free(ptemp);

	return rc;
}

int dsavrfy(const mp32barrett* p, const mp32barrett* q, const mp32number* g, const mp32number* hm, const mp32number* y, const mp32number* r, const mp32number* s)
{
	register uint32  psize = p->size;
	register uint32  qsize = q->size;
	register uint32* ptemp;
	register uint32* qtemp;
	register int rc = 0;	/* XXX shouldn't this be -1 ?*/

	if (mp32z(r->size, r->data))
		return rc;

	if (mp32gex(r->size, r->data, qsize, q->modl))
		return rc;

	if (mp32z(s->size, s->data))
		return rc;

	if (mp32gex(s->size, s->data, qsize, q->modl))
		return rc;

	ptemp = (uint32*) malloc((6*psize+2) * sizeof(*ptemp));
	if (ptemp == NULL)
		return rc;

	qtemp = (uint32*) malloc((13*qsize+11) * sizeof(*qtemp));
	if (qtemp == NULL) {
		free(ptemp);
		return rc;
	}

	{
		register uint32* pwksp = ptemp+2*psize;
		register uint32* qwksp = qtemp+2*qsize;

if (_debug) {
/*@-modfilesys@*/
fprintf(stderr, "\t                q: "),  mp32println(stderr, q->size, q->modl);
fprintf(stderr, "\t               hm: "),  mp32println(stderr, hm->size, hm->data);
fprintf(stderr, "\t                r: "),  mp32println(stderr, r->size, r->data);
fprintf(stderr, "\t                s: "),  mp32println(stderr, s->size, s->data);
/*@=modfilesys@*/
}
		// compute w = inv(s) mod q
		if (mp32binv_w(q, s->size, s->data, qtemp, qwksp))
		{
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\t w = inv(s) mod q: "),  mp32println(stderr, qsize, qtemp);
/*@=modfilesys@*/
			// compute u1 = h(m)*w mod q
			mp32bmulmod_w(q, hm->size, hm->data, qsize, qtemp, qtemp+qsize, qwksp);
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\tu1 = h(m)*w mod q: "),  mp32println(stderr, qsize, qtemp+qsize);
/*@=modfilesys@*/

			// compute u2 = r*w mod q
			mp32bmulmod_w(q, r->size, r->data, qsize, qtemp, qtemp, qwksp);
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\tu2 = r*w mod q   : "),  mp32println(stderr, qsize, qtemp);
/*@=modfilesys@*/

			// compute g^u1 mod p
			mp32bpowmod_w(p, g->size, g->data, qsize, qtemp+qsize, ptemp, pwksp);
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\t       g^u1 mod p: "),  mp32println(stderr, psize, ptemp);
/*@=modfilesys@*/

			// compute y^u2 mod p
			mp32bpowmod_w(p, y->size, y->data, qsize, qtemp, ptemp+psize, pwksp);
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\t       y^u2 mod p: "),  mp32println(stderr, psize, ptemp+psize);
/*@=modfilesys@*/

			// multiply mod p
			mp32bmulmod_w(p, psize, ptemp, psize, ptemp+psize, ptemp, pwksp);
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\t   multiply mod p: "),  mp32println(stderr, psize, ptemp);
/*@=modfilesys@*/

			// modulo q
			mp32nmod(ptemp+psize, psize, ptemp, qsize, q->modl, pwksp);
/*@-modfilesys@*/
if (_debug)
fprintf(stderr, "\tr' mod q         : "),  mp32println(stderr, psize, ptemp+psize);
/*@=modfilesys@*/

			rc = mp32eqx(r->size, r->data, psize, ptemp+psize);
		}
	}

	free(qtemp);
	free(ptemp);

	return rc;
}
