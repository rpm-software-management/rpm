/*
 * mtprng.h
 *
 * Mersenne twister pseudo-random number generator, header
 *
 * Copyright (c) 1998-2000 Virtual Unlimited B.V.
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

#ifndef _MTPRNG_H
#define _MTPRNG_H

#include "beecrypt.h"

#if WIN32
# include <Winbase.h>
#else
# if HAVE_SYNCH_H
#  include <synch.h>
# elif HAVE_PTHREAD_H
#  include <pthread.h>
# else
#  error need locking mechanism
# endif
#endif

#include "beecrypt.h"

#define N	624
#define M	397
#define K	0x9908B0DF

typedef struct
{
	#ifdef _REENTRANT
	# if WIN32
	HANDLE			lock;
	# else
	#  if HAVE_SYNCH_H
	mutex_t			lock;
	#  elif HAVE_PTHREAD_H
	pthread_mutex_t	lock;
	#  else
	#   error need locking mechanism
	#  endif
	# endif
	#endif
	uint32	state[N+1];
	uint32	left;
	uint32*	nextw;
} mtprngParam;

#ifdef __cplusplus
extern "C" {
#endif

extern BEEDLLAPI const randomGenerator mtprng;

BEEDLLAPI
int mtprngSetup  (mtprngParam*);
BEEDLLAPI
int mtprngSeed   (mtprngParam*, const uint32*, int);
BEEDLLAPI
int mtprngNext   (mtprngParam*, uint32*, int);
BEEDLLAPI
int mtprngCleanup(mtprngParam*);

#ifdef __cplusplus
}
#endif

#endif
