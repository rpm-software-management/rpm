/*
 * Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

/*!\file dsa.c
 * \brief Digital Signature Algorithm.
 * \ingroup DL_m DL_dsa_m
 */
 
#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/dsa.h"
#include "beecrypt/dldp.h"

int dsasign(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
{
	register size_t psize = p->size;
	register size_t qsize = q->size;

	register mpw* ptemp;
	register mpw* qtemp;

	register mpw* pwksp;
	register mpw* qwksp;

	register int rc = -1;

	ptemp = (mpw*) malloc((5*psize+2)*sizeof(mpw));
	if (ptemp == (mpw*) 0)
		return rc;

	qtemp = (mpw*) malloc((9*qsize+6)*sizeof(mpw));
	if (qtemp == (mpw*) 0)
	{
		free(ptemp);
		return rc;
	}

	pwksp = ptemp+psize;
	qwksp = qtemp+3*qsize;

	/* allocate r */
	mpnfree(r);
	mpnsize(r, qsize);

	/* get a random k, invertible modulo q; store k @ qtemp, inv(k) @ qtemp+qsize */
	mpbrndinv_w(q, rgc, qtemp, qtemp+qsize, qwksp);

	/* g^k mod p */
	mpbpowmod_w(p, g->size, g->data, qsize, qtemp, ptemp, pwksp);

	/* (g^k mod p) mod q - simple modulo */
	mpmod(qtemp+2*qsize, psize, ptemp, qsize, q->modl, pwksp);
	mpcopy(qsize, r->data, qtemp+psize+qsize);

	/* allocate s */
	mpnfree(s);
	mpnsize(s, qsize);

	/* x*r mod q */
	mpbmulmod_w(q, x->size, x->data, r->size, r->data, qtemp, qwksp);

	/* add h(m) mod q */
	mpbaddmod_w(q, qsize, qtemp, hm->size, hm->data, qtemp+2*qsize, qwksp);

	/* multiply inv(k) mod q */
	mpbmulmod_w(q, qsize, qtemp+qsize, qsize, qtemp+2*qsize, s->data, qwksp);

	rc = 0;

	free(qtemp);
	free(ptemp);

	return rc;
}

int dsavrfy(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
{
	register size_t psize = p->size;
	register size_t qsize = q->size;

	register mpw* ptemp;
	register mpw* qtemp;

	register mpw* pwksp;
	register mpw* qwksp;

	register int rc = 0;

	if (mpz(r->size, r->data))
		return rc;

	if (mpgex(r->size, r->data, qsize, q->modl))
		return rc;

	if (mpz(s->size, s->data))
		return rc;

	if (mpgex(s->size, s->data, qsize, q->modl))
		return rc;

	ptemp = (mpw*) malloc((6*psize+2)*sizeof(mpw));
	if (ptemp == (mpw*) 0)
		return rc;

	qtemp = (mpw*) malloc((8*qsize+6)*sizeof(mpw));
	if (qtemp == (mpw*) 0)
	{
		free(ptemp);
		return rc;
	}

	pwksp = ptemp+2*psize;
	qwksp = qtemp+2*qsize;

	mpsetx(qsize, qtemp+qsize, s->size, s->data);

	/* compute w = inv(s) mod q */
	if (mpextgcd_w(qsize, q->modl, qtemp+qsize, qtemp, qwksp))
	{
		/* compute u1 = h(m)*w mod q */
		mpbmulmod_w(q, hm->size, hm->data, qsize, qtemp, qtemp+qsize, qwksp);

		/* compute u2 = r*w mod q */
		mpbmulmod_w(q, r->size, r->data, qsize, qtemp, qtemp, qwksp);

		/* compute g^u1 mod p */
		mpbpowmod_w(p, g->size, g->data, qsize, qtemp+qsize, ptemp, pwksp);

		/* compute y^u2 mod p */
		mpbpowmod_w(p, y->size, y->data, qsize, qtemp, ptemp+psize, pwksp);

		/* multiply mod p */
		mpbmulmod_w(p, psize, ptemp, psize, ptemp+psize, ptemp, pwksp);

		/* modulo q */
		mpmod(ptemp+psize, psize, ptemp, qsize, q->modl, pwksp);

		rc = mpeqx(r->size, r->data, psize, ptemp+psize);
	}

	free(qtemp);
	free(ptemp);

	return rc;
}

int dsaparamMake(dsaparam* dp, randomGeneratorContext* rgc, size_t psize)
{
	/* psize must be >= 512 and <= 1024 */
	if ((psize < 512) || (psize > 1024))
		return -1;

	/* psize must be a multiple of 64 */
	if ((psize & 0x3f) != 0)
		return -1;

	return dldp_pgoqMake(dp, rgc, psize, 160, 1);
}
