/** \ingroup ELGAMAL_m
 * \file elgamal.c
 *
 * ElGamal signature scheme, code
 *
 * This code implements two of the six variants described:
 *
 * - ElGamal Signature variant 1: (i.e. the standard version)
 *  - Signing equation:
 *   - r = g^k mod p and
 *   - s = inv(k) * (h(m) - x*r) mod (p-1)
 *  - Verifying equation:
 *   - check 1 <= r <= (p-1)
 *   - v1 = g^h(m) mod p
 *   - v2 = y^r * r^s mod p
 *   - check v1 == v2
 *  - Simultaneous multiple exponentiation verification:
 *   - y^r * r^s * g^(p-1-h(m)) mod p = 1 or (the former is probably faster)
 *   - y^r * r^s * inv(g)^h(m) mod p = 1
 *
 * - ElGamal Signature variant 3: signing is simpler, because no inverse has to be calculated
 *  - Signing equation:
 *   - r = g^k mod p and
 *   - s = x*r + k*h(m) mod (p-1)
 *  - Verifying equation:
 *   - check 1 <= r <= (p-1)
 *   - v1 = g^s mod p
 *   - v2 = y^r * r^h(m) mod p
 *  - Simultaneous multiple exponentiation verification:
 *   - y^r * r^h(m) * g^(p-1-s) mod p = 1 (one of the exponents is significantly smaller, i.e. h(m))
 *
 * For more information on this algorithm, see:
 *  "Handbook of Applied Cryptography"
 *  11.5.2 "The ElGamal signature scheme", p. 454-459
 */

/*
 * Copyright (c) 1999, 2000, 2001 Virtual Unlimited B.V.
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
 */

#define BEECRYPT_DLL_EXPORT

#include "elgamal.h"
#include "dldp.h"
#include "mp32.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

int elgv1sign(const mp32barrett* p, const mp32barrett* n, const mp32number* g, randomGeneratorContext* rgc, const mp32number* hm, const mp32number* x, mp32number* r, mp32number* s)
{
	register uint32  size = p->size;
	register uint32* temp = (uint32*) malloc((8*size+6)*sizeof(uint32));

	if (temp)
	{
		/* get a random k, invertible modulo (p-1) */
		mp32brndinv_w(n, rgc, temp, temp+size, temp+2*size);

		/* compute r = g^k mod p */
		mp32nfree(r);
		mp32nsize(r, size);
		mp32bpowmod_w(p, g->size, g->data, size, temp, r->data, temp+2*size);

		/* compute x*r mod n */
		mp32bmulmod_w(n, x->size, x->data, r->size, r->data, temp, temp+2*size);

		/* compute -(x*r) mod n */
		mp32neg(size, temp);
		(void) mp32add(size, temp, n->modl);

		/* compute h(m) - x*r mod n */
		mp32baddmod_w(n, hm->size, hm->data, size, temp, temp, temp+2*size);

		/* compute s = inv(k)*(h(m) - x*r) mod n */
		mp32nfree(s);
		mp32nsize(s, size);
		mp32bmulmod_w(n, size, temp, size, temp+size, s->data, temp+2*size);

		free(temp);

		return 0;
	}
	return -1;
}

int elgv1vrfy(const mp32barrett* p, const mp32barrett* n, const mp32number* g, const mp32number* hm, const mp32number* y, const mp32number* r, const mp32number* s)
{
	register uint32  size = p->size;
	register uint32* temp;

	if (mp32z(r->size, r->data))
		return 0;

	if (mp32gex(r->size, r->data, size, p->modl))
		return 0;

	if (mp32z(s->size, s->data))
		return 0;

	if (mp32gex(s->size, s->data, n->size, n->modl))
		return 0;

	temp = (uint32*) malloc((6*size+2)*sizeof(uint32));

	if (temp)
	{
		register int rc;

		/* compute u1 = y^r mod p */
		mp32bpowmod_w(p, y->size, y->data, r->size, r->data, temp, temp+2*size);

		/* compute u2 = r^s mod p */
		mp32bpowmod_w(p, r->size, r->data, s->size, s->data, temp+size, temp+2*size);

		/* compute v2 = u1*u2 mod p */
		mp32bmulmod_w(p, size, temp, size, temp+size, temp+size, temp+2*size);

		/* compute v1 = g^h(m) mod p */
		mp32bpowmod_w(p, g->size, g->data, hm->size, hm->data, temp, temp+2*size);

		rc = mp32eq(size, temp, temp+size);

		free(temp);

		return rc;
	}
	return 0;
}

int elgv3sign(const mp32barrett* p, const mp32barrett* n, const mp32number* g, randomGeneratorContext* rgc, const mp32number* hm, const mp32number* x, mp32number* r, mp32number* s)
{
	register uint32  size = p->size;
	register uint32* temp = (uint32*) malloc((6*size+2)*sizeof(uint32));

	if (temp)
	{
		/* get a random k */
		mp32brnd_w(p, rgc, temp, temp+2*size);

		/* compute r = g^k mod p */
		mp32nfree(r);
		mp32nsize(r, size);
		mp32bpowmod_w(p, g->size, g->data, size, temp, r->data, temp+2*size);

		/* compute u1 = x*r mod n */
		mp32bmulmod_w(n, x->size, x->data, size, r->data, temp+size, temp+2*size);

		/* compute u2 = k*h(m) mod n */
		mp32bmulmod_w(n, size, temp, hm->size, hm->data, temp, temp+2*size);

		/* compute s = u1+u2 mod n */
		mp32nfree(s);
		mp32nsize(s, n->size);
		mp32baddmod_w(n, size, temp, size, temp+size, s->data, temp+2*size);

		free(temp);

		return 0;
	}
	return -1;
}

int elgv3vrfy(const mp32barrett* p, const mp32barrett* n, const mp32number* g, const mp32number* hm, const mp32number* y, const mp32number* r, const mp32number* s)
{
	register uint32  size = p->size;
	register uint32* temp;

	if (mp32z(r->size, r->data))
		return 0;

	if (mp32gex(r->size, r->data, size, p->modl))
		return 0;

	if (mp32z(s->size, s->data))
		return 0;

	if (mp32gex(s->size, s->data, n->size, n->modl))
		return 0;

	temp = (uint32*) malloc((6*size+2)*sizeof(uint32));

	if (temp)
	{
		register int rc;

		/* compute u1 = y^r mod p */
		mp32bpowmod_w(p, y->size, y->data, r->size, r->data, temp, temp+2*size);

		/* compute u2 = r^h(m) mod p */
		mp32bpowmod_w(p, r->size, r->data, hm->size, hm->data, temp+size, temp+2*size);

		/* compute v2 = u1*u2 mod p */
		mp32bmulmod_w(p, size, temp, size, temp+size, temp+size, temp+2*size);

		/* compute v1 = g^s mod p */
		mp32bpowmod_w(p, g->size, g->data, s->size, s->data, temp, temp+2*size);

		rc = mp32eq(size, temp, temp+size);

		free(temp);

		return rc;
	}
	return 0;
}
