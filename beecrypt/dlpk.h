/** \ingroup DL_m
 * \file dlpk.h
 *
 * Discrete Logarithm Public Key, header.
 */

/*
 * Copyright (c) 2000 Virtual Unlimited B.V.
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

#ifndef _DLPK_H
#define _DLPK_H

#include "dldp.h"

/**
 */
typedef struct
{
	dldp_p param;
	mp32number y;
} dlpk_p;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEEDLLAPI /*@unused@*/
int dlpk_pInit(dlpk_p* pk)
	/*@modifies pk */;

/**
 */
BEEDLLAPI /*@unused@*/
int dlpk_pFree(dlpk_p* pk)
	/*@modifies pk */;

/**
 */
BEEDLLAPI /*@unused@*/
int dlpk_pCopy(dlpk_p* dst, const dlpk_p* src)
	/*@modifies dst */;

/**
 */
BEEDLLAPI /*@unused@*/
int  dlpk_pEqual(const dlpk_p* a, const dlpk_p* b)
	/*@*/;

/**
 */
BEEDLLAPI /*@unused@*/
int  dlpk_pgoqValidate(const dlpk_p* pk, randomGeneratorContext* rgc, int cofactor)
	/*@modifies rgc @*/;

/**
 */
BEEDLLAPI /*@unused@*/
int  dlpk_pgonValidate(const dlpk_p* pk, randomGeneratorContext* rgc)
	/*@modifies rgc @*/;

#ifdef __cplusplus
}
#endif

#endif
