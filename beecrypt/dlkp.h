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

/*!\file dlkp.h
 * \brief Discrete Logarithm keypair, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m
 */

#ifndef _DLKP_H
#define _DLKP_H

#include "beecrypt/dlpk.h"

/*!\ingroup DL_m
 */
#ifdef __cplusplus
struct BEECRYPTAPI dlkp_p
#else
struct _dlkp_p
#endif
{
	dldp_p param;
	mpnumber y;
	mpnumber x;

	#ifdef __cplusplus
	dlkp_p();
	dlkp_p(const dlkp_p&);
	~dlkp_p();
	#endif
};

#ifndef __cplusplus
typedef struct _dlkp_p dlkp_p;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int dlkp_pPair(dlkp_p*, randomGeneratorContext*, const dldp_p*);
BEECRYPTAPI
int dlkp_pInit(dlkp_p*);
BEECRYPTAPI
int dlkp_pFree(dlkp_p*);
BEECRYPTAPI
int dlkp_pCopy(dlkp_p*, const dlkp_p*);

#ifdef __cplusplus
}
#endif

#endif
