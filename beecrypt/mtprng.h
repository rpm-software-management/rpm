/*
 * Copyright (c) 1998, 1999, 2000, 2003 Virtual Unlimited B.V.
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

/*!\file mtprng.h
 * \brief Mersenne Twister pseudo-random number generator, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup PRNG_m
 */

#ifndef _MTPRNG_H
#define _MTPRNG_H

#include "beecrypt/beecrypt.h"

#ifdef _REENTRANT
# if WIN32
#  include <windows.h>
#  include <winbase.h>
# endif
#endif

#define N	624
#define M	397
#define K	0x9908B0DFU

/*
 */
typedef struct
{
	#ifdef _REENTRANT
	bc_mutex_t	lock;
	#endif
	uint32_t	state[N+1];
	uint32_t	left;
	uint32_t*	nextw;
} mtprngParam;

#ifdef __cplusplus
extern "C" {
#endif

/*
 */
extern BEECRYPTAPI const randomGenerator mtprng;

/*
 */
BEECRYPTAPI
int mtprngSetup  (mtprngParam* mp);

/*
 */
BEECRYPTAPI
int mtprngSeed   (mtprngParam* mp, const byte* data, size_t size);

/*
 */
BEECRYPTAPI
int mtprngNext   (mtprngParam* mp, byte* data, size_t size);

/*
 */
BEECRYPTAPI
int mtprngCleanup(mtprngParam* mp);

#ifdef __cplusplus
}
#endif

#endif
