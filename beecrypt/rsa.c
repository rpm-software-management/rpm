/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file rsa.c
 * \brief RSA algorithm.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup IF_m IF_rsa_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/rsa.h"

int rsapub(const mpbarrett* n, const mpnumber* e,
           const mpnumber* m, mpnumber* c)
{
	register size_t size = n->size;
	register mpw* temp;

	if (mpgex(m->size, m->data, n->size, n->modl))
		return -1;

	temp = (mpw*) malloc((4*size+2)*sizeof(mpw));

	if (temp)
	{
		mpnsize(c, size);

		mpbpowmod_w(n, m->size, m->data, e->size, e->data, c->data, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int rsapri(const mpbarrett* n, const mpnumber* d,
           const mpnumber* c, mpnumber* m)
{
	register size_t size = n->size;
	register mpw* temp;

	if (mpgex(c->size, c->data, n->size, n->modl))
		return -1;

	temp = (mpw*) malloc((4*size+2)*sizeof(mpw));

	if (temp)
	{
		mpnsize(m, size);
		mpbpowmod_w(n, c->size, c->data, d->size, d->data, m->data, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int rsapricrt(const mpbarrett* n, const mpbarrett* p, const mpbarrett* q,
              const mpnumber* dp, const mpnumber* dq, const mpnumber* qi,
              const mpnumber* c, mpnumber* m)
{
	register size_t nsize = n->size;
	register size_t psize = p->size;
	register size_t qsize = q->size;

	register mpw* ptemp;
	register mpw* qtemp;

	if (mpgex(c->size, c->data, n->size, n->modl))
		return -1;

	ptemp = (mpw*) malloc((6*psize+2)*sizeof(mpw));
	if (ptemp == (mpw*) 0)
		return -1;

	qtemp = (mpw*) malloc((6*qsize+2)*sizeof(mpw));
	if (qtemp == (mpw*) 0)
	{
		free(ptemp);
		return -1;
	}

	/* resize c for powmod p */
	mpsetx(psize*2, ptemp, c->size, c->data);

	/* reduce modulo p before we powmod */
	mpbmod_w(p, ptemp, ptemp+psize, ptemp+2*psize);

	/* compute j1 = c^dp mod p, store @ ptemp */
	mpbpowmod_w(p, psize, ptemp+psize, dp->size, dp->data, ptemp, ptemp+2*psize);

	/* resize c for powmod q */
	mpsetx(qsize*2, qtemp, c->size, c->data);

	/* reduce modulo q before we powmod */
	mpbmod_w(q, qtemp, qtemp+qsize, qtemp+2*qsize);

	/* compute j2 = c^dq mod q, store @ qtemp */
	mpbpowmod_w(q, qsize, qtemp+qsize, dq->size, dq->data, qtemp, qtemp+2*qsize);

	/* compute j1-j2 mod p, store @ ptemp */
	mpbsubmod_w(p, psize, ptemp, qsize, qtemp, ptemp, ptemp+2*psize);

	/* compute h = c*(j1-j2) mod p, store @ ptemp */
	mpbmulmod_w(p, psize, ptemp, psize, qi->data, ptemp, ptemp+2*psize);

	/* make sure the message gets the proper size */
	mpnsize(m, nsize);

	/* compute m = h*q + j2 */
	mpmul(m->data, psize, ptemp, qsize, q->modl);
	mpaddx(nsize, m->data, qsize, qtemp);

	free(ptemp);
	free(qtemp);

	return 0;
}

int rsavrfy(const mpbarrett* n, const mpnumber* e,
            const mpnumber* m, const mpnumber* c)
{
	int rc;
	register size_t size = n->size;

	register mpw* temp;

	if (mpgex(m->size, m->data, n->size, n->modl))
		return -1;

	if (mpgex(c->size, c->data, n->size, n->modl))
		return 0;

	temp = (mpw*) malloc((5*size+2)*sizeof(mpw));

	if (temp)
	{
		mpbpowmod_w(n, m->size, m->data, e->size, e->data, temp, temp+size);

		rc = mpeqx(size, temp, c->size, c->data);

		free(temp);

		return rc;
	}

	return 0;
}
