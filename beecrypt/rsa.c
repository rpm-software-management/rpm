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

#define BEECRYPT_DLL_EXPORT

#include "rsa.h"
#include "mp32.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

int rsapri(const rsakp* kp, const mp32number* m, mp32number* c)
{
	register uint32  size = kp->n.size;
	register uint32* temp = (uint32*) malloc((4*size+2)*sizeof(uint32));

	if (temp)
	{
		mp32nsize(c, size);
		mp32bpowmod_w(&kp->n, m->size, m->data, kp->d.size, kp->d.data, c->data, temp);

		free(temp);

		return 0;
	}
	return -1;
}

/*@-nullpass -nullptrarith @*/ /* temp may be NULL */
/* this routine doesn't work yet: needs debugging! */
int rsapricrt(const rsakp* kp, const mp32number* m, mp32number* c)
{
	register uint32  nsize = kp->n.size;
	register uint32  psize = kp->p.size;
	register uint32  qsize = kp->q.size;
	register uint32* temp = (uint32*) malloc((psize+qsize+(5*nsize+6))*sizeof(uint32));
	register uint32* wksp = temp+psize+qsize+nsize;

	/* compute j1 = m^d1 mod p */
	if (mp32gex(psize, kp->p.modl, m->size, m->data))
	{
		mp32setx(nsize, temp+psize+qsize, m->size, m->data);
		/*@-compdef@*/ /* LCL: temp+psize+qsize */
		mp32bmod_w(&kp->p, temp+psize+qsize, temp, wksp);
		/*@=compdef@*/
	}
	else
		mp32setx(psize, temp, m->size, m->data);

	mp32bpowmod_w(&kp->p, psize, temp, kp->d1.size, kp->d1.data, temp, wksp);
	
	/* compute j2 = m^d2 mod q */
	if (mp32gex(qsize, kp->q.modl, m->size, m->data))
	{
		mp32setx(nsize, temp+psize+qsize, m->size, m->data);
		/*@-compdef@*/ /* LCL: temp+psize+qsize */
		mp32bmod_w(&kp->q, temp+psize+qsize, temp+psize, wksp);
		/*@=compdef@*/
	}
	else
		mp32setx(qsize, temp+psize, m->size, m->data);

	mp32bpowmod_w(&kp->q, qsize, temp+psize, kp->d2.size, kp->d2.data, temp+psize, wksp);

	/* compute j1-j2 */
	(void) mp32subx(psize, temp, qsize, temp+psize);

	/* compute h = c*(j1-j2) mod p */
	mp32bmulmod_w(&kp->p, psize, temp, psize, kp->c.data, temp, wksp);

	/* make sure the signature gets the proper size */
	mp32nsize(c, nsize);

	/* compute s = h*q + j2 */
	mp32mul(c->data, psize, temp, qsize, kp->q.modl);
	(void) mp32addx(nsize, c->data, qsize, temp+psize);

	free(temp);

	return -1;
}
/*@=nullpass =nullptrarith @*/

/**
 * @return	1 if signature verifies, 0 otherwise (can also indicate errors)
 */
int rsavrfy(const rsapk* pk, const mp32number* m, const mp32number* c)
{
	int rc;
	register uint32  size = pk->n.size;
	register uint32* temp = (uint32*) malloc((5*size+2)*sizeof(uint32));

	if (temp)
	{
		mp32bpowmod_w(&pk->n, c->size, c->data, pk->e.size, pk->e.data, temp, temp+size);

		rc = mp32eqx(size, temp, m->size, m->data);

		free(temp);

		return rc;
	}
	return 0;
}
