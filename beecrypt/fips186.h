/*
 * fips186.h
 *
 * FIPS186 pseudo-random generator, with SHA-1 as H function, header
 *
 * Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

#ifndef _FIPS186_H
#define _FIPS186_H

#include "beecrypt.h"

#ifdef _REENTRANT
# if WIN32
#  include <windows.h>
#  include <winbase.h>
# else
#  if HAVE_SYNCH_H
#   include <synch.h>
#  elif HAVE_PTHREAD_H
#   include <pthread.h>
#  else
#   error need locking mechanism
#  endif
# endif
#endif

#include "beecrypt.h"
#include "fips180.h"

#define FIPS186_STATE_SIZE	16

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
	sha1Param	param;
	uint32		state[FIPS186_STATE_SIZE];
	int			digestsize;
} fips186Param;

#ifdef __cplusplus
extern "C" {
#endif

extern BEEDLLAPI const randomGenerator fips186prng;

BEEDLLAPI
int fips186Setup  (fips186Param* fp)
	/*@modifies fp */;
BEEDLLAPI
int fips186Seed   (fips186Param* fp, const uint32* data, int size)
	/*@modifies fp */;
BEEDLLAPI
int fips186Next   (fips186Param* fp, uint32* data, int size)
	/*@modifies fp, data */;
BEEDLLAPI
int fips186Cleanup(fips186Param* fp)
	/*@modifies fp */;

#ifdef __cplusplus
}
#endif

#endif
