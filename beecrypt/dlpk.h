/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
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

/*!\file dlpk.h
 * \brief Discrete Logarithm public key, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m
 */

#ifndef _DLPK_H
#define _DLPK_H

#include "beecrypt/dldp.h"

/*!\ingroup DL_m
 */
#ifdef __cplusplus
struct BEECRYPTAPI dlpk_p
#else
struct _dlpk_p
#endif
{
	dldp_p param;
	mpnumber y;
#ifdef __cplusplus
	dlpk_p();
	dlpk_p(const dlpk_p&);
	~dlpk_p();
#endif
};

#ifndef __cplusplus
typedef struct _dlpk_p dlpk_p;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int dlpk_pInit(dlpk_p*);
BEECRYPTAPI
int dlpk_pFree(dlpk_p*);
BEECRYPTAPI
int dlpk_pCopy(dlpk_p*, const dlpk_p*);

BEECRYPTAPI
int  dlpk_pEqual(const dlpk_p*, const dlpk_p*);

BEECRYPTAPI
int  dlpk_pgoqValidate(const dlpk_p*, randomGeneratorContext*, int cofactor);
BEECRYPTAPI
int  dlpk_pgonValidate(const dlpk_p*, randomGeneratorContext*);

#ifdef __cplusplus
}
#endif

#endif
