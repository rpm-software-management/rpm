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

/*!\file rsapk.h
 * \brief RSA public key, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup IF_m IF_rsa_m
 */

#ifndef _RSAPK_H
#define _RSAPK_H

#include "beecrypt/mpbarrett.h"

#ifdef __cplusplus
struct BEECRYPTAPI rsapk
#else
struct _rsapk
#endif
{
	mpbarrett n;
	mpnumber e;
	#ifdef __cplusplus
	rsapk();
	rsapk(const rsapk&);
	~rsapk();
	#endif
};

#ifndef __cplusplus
typedef struct _rsapk rsapk;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int rsapkInit(rsapk*);
BEECRYPTAPI
int rsapkFree(rsapk*);
BEECRYPTAPI
int rsapkCopy(rsapk*, const rsapk*);

#ifdef __cplusplus
}
#endif

#endif
