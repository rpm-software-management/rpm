/** \ingroup DL_m
 * \file dldp.h
 *
 * Discrete Logarithm Domain Parameters, header.
 */

/*
 * <conformance statement for IEEE P1363 needed here>
 *
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

#ifndef _DLDP_H
#define _DLDP_H

#include "beecrypt.h"
#include "mp32barrett.h"

/**
 * Discrete Logarithm Domain Parameters - Prime
 *
 * Standard definition where p = qr+1; in case where p=2q+1, r=2
 *
 * In IEEE P1363 naming is p = rk+1
 *
 * Hence, IEEE prime r = q and cofactor k = r
 *
 * Make sure q is large enough to foil Pohlig-Hellman attacks
 *  See: "Handbook of Applied Cryptography", Chapter 3.6.4
 *
 * g is either a generator of a subgroup of order q, or a generator of order
 *  n = (p-1)
 */
typedef struct
{
	mp32barrett p;
	mp32barrett q;
	mp32number  r;
	mp32number  g;
	mp32barrett n;
} dldp_p;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEEDLLAPI
int dldp_pInit(dldp_p* dp)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n @*/;

/**
 */
BEEDLLAPI
int dldp_pFree(/*@special@*/ dldp_p* dp)
	/*@releases dp->p.modl, dp->q.modl, dp->n.modl @*/
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n @*/;

/**
 */
BEEDLLAPI
int dldp_pCopy(dldp_p* dst, const dldp_p* src)
	/*@modifies dst @*/;

/*
 * Functions for generating keys
 */

/**
 */
BEEDLLAPI /*@unused@*/
int dldp_pPrivate(const dldp_p* dp, randomGeneratorContext* rgc, mp32number* x)
	/*@modifies rgc, x @*/;

/**
 */
BEEDLLAPI /*@unused@*/
int dldp_pPublic(const dldp_p* dp, const mp32number* x, mp32number* y)
	/*@modifies y @*/;

/**
 */
BEEDLLAPI
int dldp_pPair(const dldp_p* dp, randomGeneratorContext* rgc, mp32number* x, mp32number* y)
	/*@modifies rgc, x, y @*/;

/*
 * Function for comparing domain parameters
 */


/**
 */
BEEDLLAPI
int  dldp_pEqual(const dldp_p* a, const dldp_p* b)
	/*@*/;

/*
 * Functions for generating and validating dldp_pgoq variant domain parameters
 */


/**
 */
BEEDLLAPI
int dldp_pgoqMake(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize, uint32 qsize, int cofactor)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n, rgc @*/;

/**
 */
BEEDLLAPI /*@unused@*/
int dldp_pgoqMakeSafe(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n, rgc @*/;

/**
 */
BEEDLLAPI /*@unused@*/
int dldp_pgoqGenerator(dldp_p* dp, randomGeneratorContext* rgc)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n, rgc @*/;

/**
 */
BEEDLLAPI
int  dldp_pgoqValidate(const dldp_p*, randomGeneratorContext* rgc, int cofactor)
	/*@modifies rgc @*/;

/*
 * Functions for generating and validating dldp_pgon variant domain parameters
 */


/**
 */
BEEDLLAPI
int dldp_pgonMake(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize, uint32 qsize)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n, rgc @*/;

/**
 */
BEEDLLAPI /*@unused@*/
int dldp_pgonMakeSafe(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n, rgc @*/;

/**
 */
BEEDLLAPI /*@unused@*/
int dldp_pgonGenerator(dldp_p* dp, randomGeneratorContext* rgc)
	/*@modifies dp->p, dp->q, dp->r, dp->g, dp->n, rgc @*/;

/**
 */
BEEDLLAPI
int  dldp_pgonValidate(const dldp_p* dp, randomGeneratorContext* rgc)
	/*@modifies rgc @*/;

#ifdef __cplusplus
}
#endif

#endif
