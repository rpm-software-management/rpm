/** \ingroup DL_m
 * \file dlkp.h
 *
 * Discrete Logarithm Keypair, header.
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

#ifndef _DLKP_H
#define _DLKP_H

#include "dlpk.h"

/**
 */
typedef struct
{
	dldp_p param;
	mp32number y;
	mp32number x;
} dlkp_p;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEEDLLAPI /*@unused@*/
int dlkp_pPair(dlkp_p* kp, randomGeneratorContext* rgc, const dldp_p* param)
	/*@modifies kp, rgc */;

/**
 */
BEEDLLAPI /*@unused@*/
int dlkp_pInit(dlkp_p* kp)
	/*@modifies kp */;

/**
 */
BEEDLLAPI /*@unused@*/
int dlkp_pFree(dlkp_p* kp)
	/*@modifies kp */;

/**
 */
BEEDLLAPI /*@unused@*/
int dlkp_pCopy(dlkp_p* dst, const dlkp_p* src)
	/*@modifies dst */;

#ifdef __cplusplus
}
#endif

#endif
