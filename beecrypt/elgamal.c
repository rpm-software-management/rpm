/*
 * Copyright (c) 1999, 2000, 2001, 2002 Virtual Unlimited B.V.
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
 */
 
/*!\file elgamal.c
 * \brief ElGamal algorithm.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/elgamal.h"
#include "beecrypt/dldp.h"

int elgv1sign(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
{
	register size_t size = p->size;
	register mpw* temp = (mpw*) malloc((8*size+6)*sizeof(mpw));

	if (temp)
	{
		/* get a random k, invertible modulo (p-1) */
		mpbrndinv_w(n, rgc, temp, temp+size, temp+2*size);

		/* compute r = g^k mod p */
		mpnfree(r);
		mpnsize(r, size);
		mpbpowmod_w(p, g->size, g->data, size, temp, r->data, temp+2*size);

		/* compute x*r mod n */
		mpbmulmod_w(n, x->size, x->data, r->size, r->data, temp, temp+2*size);

		/* compute -(x*r) mod n */
		mpneg(size, temp);
		mpadd(size, temp, n->modl);

		/* compute h(m) - x*r mod n */
		mpbaddmod_w(n, hm->size, hm->data, size, temp, temp, temp+2*size);

		/* compute s = inv(k)*(h(m) - x*r) mod n */
		mpnfree(s);
		mpnsize(s, size);
		mpbmulmod_w(n, size, temp, size, temp+size, s->data, temp+2*size);

		free(temp);

		return 0;
	}
	return -1;
}

int elgv1vrfy(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
{
	register size_t size = p->size;
	register mpw* temp;

	if (mpz(r->size, r->data))
		return 0;

	if (mpgex(r->size, r->data, size, p->modl))
		return 0;

	if (mpz(s->size, s->data))
		return 0;

	if (mpgex(s->size, s->data, n->size, n->modl))
		return 0;

	temp = (mpw*) malloc((6*size+2)*sizeof(mpw));

	if (temp)
	{
		register int rc;

		/* compute u1 = y^r mod p */
		mpbpowmod_w(p, y->size, y->data, r->size, r->data, temp, temp+2*size);

		/* compute u2 = r^s mod p */
		mpbpowmod_w(p, r->size, r->data, s->size, s->data, temp+size, temp+2*size);

		/* compute v1 = u1*u2 mod p */
		mpbmulmod_w(p, size, temp, size, temp+size, temp+size, temp+2*size);

		/* compute v2 = g^h(m) mod p */
		mpbpowmod_w(p, g->size, g->data, hm->size, hm->data, temp, temp+2*size);

		rc = mpeq(size, temp, temp+size);

		free(temp);

		return rc;
	}
	return 0;
}

int elgv3sign(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
{
	register size_t size = p->size;
	register mpw* temp = (mpw*) malloc((6*size+2)*sizeof(mpw));

	if (temp)
	{
		/* get a random k */
		mpbrnd_w(p, rgc, temp, temp+2*size);

		/* compute r = g^k mod p */
		mpnfree(r);
		mpnsize(r, size);
		mpbpowmod_w(p, g->size, g->data, size, temp, r->data, temp+2*size);

		/* compute u1 = x*r mod n */
		mpbmulmod_w(n, x->size, x->data, size, r->data, temp+size, temp+2*size);

		/* compute u2 = k*h(m) mod n */
		mpbmulmod_w(n, size, temp, hm->size, hm->data, temp, temp+2*size);

		/* compute s = u1+u2 mod n */
		mpnfree(s);
		mpnsize(s, n->size);
		mpbaddmod_w(n, size, temp, size, temp+size, s->data, temp+2*size);

		free(temp);

		return 0;
	}
	return -1;
}

int elgv3vrfy(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
{
	register size_t size = p->size;
	register mpw* temp;

	if (mpz(r->size, r->data))
		return 0;

	if (mpgex(r->size, r->data, size, p->modl))
		return 0;

	if (mpz(s->size, s->data))
		return 0;

	if (mpgex(s->size, s->data, n->size, n->modl))
		return 0;

	temp = (mpw*) malloc((6*size+2)*sizeof(mpw));

	if (temp)
	{
		register int rc;

		/* compute u1 = y^r mod p */
		mpbpowmod_w(p, y->size, y->data, r->size, r->data, temp, temp+2*size);

		/* compute u2 = r^h(m) mod p */
		mpbpowmod_w(p, r->size, r->data, hm->size, hm->data, temp+size, temp+2*size);

		/* compute v2 = u1*u2 mod p */
		mpbmulmod_w(p, size, temp, size, temp+size, temp+size, temp+2*size);

		/* compute v1 = g^s mod p */
		mpbpowmod_w(p, g->size, g->data, s->size, s->data, temp, temp+2*size);

		rc = mpeq(size, temp, temp+size);

		free(temp);

		return rc;
	}
	return 0;
}

/*!\}
 */
