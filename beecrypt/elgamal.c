/*
 * elgamal.c
 *
 * ElGamal signature scheme, code
 *
 * For more information on this algorithm, see:
 *  "Handbook of Applied Cryptography"
 *  11.5.2 "The ElGamal signature scheme", p. 454-459
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
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
 * This code implements two of the six variants described:
 *
 * ElGamal Signature variant 1: (i.e. the standard version)
 *  Signing equation:
 *   r = g^k mod p and
 *   s = inv(k) * (h(m) - x*r) mod (p-1)
 *  Verifying equation:
 *   check 1 <= r <= (p-1)
 *   v1 = y^r * r^s mod p
 *   v2 = g^h(m) mod p
 *   check v1 == v2
 *  Simultaneous multiple exponentiation verification:
 *   y^r * r^s * g^(p-1-h(m)) mod p = 1 or (the former is probably faster)
 *   y^r * r^s * inv(g)^h(m) mod p = 1
 *
 * ElGamal Signature variant 3: signing is simpler, because no inverse has to be calculated
 *  Signing equation:
 *   r = g^k mod p and
 *   s = x*r + k*h(m) mod (p-1)
 *  Verifying equation:
 *   check 1 <= r <= (p-1)
 *   v1 = g^s mod p
 *   v2 = y^r * r^h(m) mod p
 *  Simultaneous multiple exponentiation verification:
 *   y^r * r^h(m) * g^(p-1-s) mod p = 1 (one of the exponents is significantly smaller, i.e. h(m))
 *
 */
 
#define BEECRYPT_DLL_EXPORT

#include "elgamal.h"
#include "mp32.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

void elgv3sign(const mp32barrett* p, const mp32barrett* n, const mp32number* g, randomGeneratorContext* rc, const mp32number* hm, const mp32number* x, mp32number* r, mp32number* s)
{
	register uint32  size   = p->size;
	register uint32* kdata  = p->wksp+size*4+4; /* leave enough workspace for a powmod operation */
	register uint32* u1data = n->wksp+size*4+4; /* leave enough workspace for a mulmod and addmod operation */
	register uint32* u2data = u1data+size;

	/* get a random k */
	mp32brnd(p, rc);
	mp32copy(size, kdata, p->data);

	/* compute r = g^k mod p */
	mp32bpowmod(p, g->size, g->data, size, kdata);
	mp32nset(r, size, p->data);

	/* compute u1 = x*r mod n */
	mp32bmulmodres(n, u1data, x->size, x->data, size, p->data);

	/* compute u2 = k*h(m) mod n */
	mp32bmulmodres(n, u2data, hm->size, hm->data, size, kdata);

	/* compute s = u1+u2 mod n */
	mp32baddmod(n, size, u1data, size, u2data);
	mp32nset(s, size, n->data);
}

int elgv3vrfy(const mp32barrett* p, const mp32barrett* n, const mp32number* g, const mp32number* hm, const mp32number* y, const mp32number* r, const mp32number* s)
{
	register uint32  size =   p->size;
	register uint32* v1data = p->wksp+size*4+4;
	register uint32* u1data = v1data+size;

	if (mp32z(r->size, r->data))
		return 0;

	if (mp32gex(r->size, r->data, size, p->modl))
		return 0;

	if (mp32z(s->size, s->data))
		return 0;

	if (mp32gex(s->size, s->data, n->size, n->modl))
		return 0;

	#ifdef COMING_SOON
	/* here we need the simultaneous multiple exponentiation with three pairs */
	#endif

	/* compute v1 = g^s mod p */
	mp32bpowmod(p, g->size, g->data, s->size, s->data);
	mp32copy(size, v1data, p->data);

	/* compute u1 = y^r mod p */
	mp32bpowmod(p, y->size, y->data, r->size, r->data);
	mp32copy(size, u1data, p->data);

	/* compute u2 = r^h(m) mod p */
	mp32bpowmod(p, r->size, r->data, hm->size, hm->data);

	/* compute v2 = u1*u2 mod p */
	mp32bmulmod(p, size, p->data, size, u1data);

	return mp32eq(size, v1data, p->data);
}
