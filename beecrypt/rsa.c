/** \ingroup RSA_m
 * \file rsa.c
 *
 * RSA Encryption & signature scheme, code.
 */

/*
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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
 */

#include "system.h"
#include "rsa.h"
#include "mp32.h"
#include "debug.h"

int rsapri(const rsakp* kp, const mp32number* m, mp32number* c)
{
	register uint32  size = kp->n.size;
	register uint32* temp = (uint32*) malloc((4*size+2) * sizeof(*temp));

	if (temp)
	{
		mp32nsize(c, size);
		mp32bpowmod_w(&kp->n, m->size, m->data, kp->d.size, kp->d.data, c->data, temp);

		free(temp);

		return 0;
	}
	return -1;
}


int rsapricrt(const rsakp* kp, const mp32number* m, mp32number* c)
{
	register uint32  nsize = kp->n.size;
	register uint32  psize = kp->p.size;
	register uint32  qsize = kp->q.size;

	register uint32* ptemp;
	register uint32* qtemp;

	ptemp = (uint32*) malloc((6*psize+2)*sizeof(uint32));
	if (ptemp == (uint32*) 0)
		return -1;

	qtemp = (uint32*) malloc((6*qsize+2)*sizeof(uint32));
	if (qtemp == (uint32*) 0)
	{
		free(ptemp);
		return -1;
	}

	/* m must be small enough to be exponentiated modulo p and q */
	if (m->size > psize || m->size > qsize)
		return -1;

	/* resize m for powmod p */
	mp32setx(psize, ptemp+psize, m->size, m->data);

	/* compute j1 = m^d1 mod p, store @ ptemp */
	mp32bpowmod_w(&kp->p, psize, ptemp+psize, kp->d1.size, kp->d1.data, ptemp, ptemp+2*psize);

	/* resize m for powmod p */
	mp32setx(qsize, qtemp+psize, m->size, m->data);

	/* compute j2 = m^d2 mod q, store @ qtemp */
	mp32bpowmod_w(&kp->q, qsize, qtemp+psize, kp->d2.size, kp->d2.data, qtemp, qtemp+2*qsize);

	/* compute j1-j2 mod p, store @ ptemp */
	mp32bsubmod_w(&kp->p, psize, ptemp, qsize, qtemp, ptemp, ptemp+2*psize);

	/* compute h = c*(j1-j2) mod p, store @ ptemp */
	mp32bmulmod_w(&kp->p, psize, ptemp, psize, kp->c.data, ptemp, ptemp+2*psize);

	/* make sure the signature gets the proper size */
	mp32nsize(c, nsize);

	/* compute s = h*q + j2 */
	mp32mul(c->data, psize, ptemp, qsize, kp->q.modl);
	mp32addx(nsize, c->data, qsize, qtemp);

	free(ptemp);
	free(qtemp);

	return 0;
}

/**
 * @return	1 if signature verifies, 0 otherwise (can also indicate errors)
 */
int rsavrfy(const rsapk* pk, const mp32number* m, const mp32number* c)
{
	int rc;
	register uint32  size = pk->n.size;
	register uint32* temp = (uint32*) malloc((5*size+2) * sizeof(*temp));

	if (temp)
	{
		mp32bpowmod_w(&pk->n, c->size, c->data, pk->e.size, pk->e.data, temp, temp+size);

		rc = mp32eqx(size, temp, m->size, m->data);

		free(temp);

		return rc;
	}
	return 0;
}
