/**
 * \file mtprng.c
 *
 * Mersenne Twister pseudo-random number generator, code.
 *
 * Developed by Makoto Matsumoto and Takuji Nishimura
 *
 * For more information, see:
 *  http://www.math.keio.ac.jp/~matumoto/emt.html
 *
 * Adapted from optimized code by Shawn J. Cokus <cokus@math.washington.edu>
 *
 * Note: this generator has a very long period, passes statistical test, but
 * needs more study to determine whether it is cryptographically strong enough.
 *
 */

/* Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

#include "mtprng.h"
#include "mp32.h"
#include "mp32opt.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

#define hiBit(a)		((a) & 0x80000000)
#define loBit(a)		((a) & 0x1)
#define loBits(a)		((a) & 0x7FFFFFFF)
#define mixBits(a, b)	(hiBit(a) | loBits(b))

/*@-sizeoftype@*/
const randomGenerator mtprng = { "Mersenne Twister", sizeof(mtprngParam), (randomGeneratorSetup) mtprngSetup, (randomGeneratorSeed) mtprngSeed, (randomGeneratorNext) mtprngNext, (randomGeneratorCleanup) mtprngCleanup };
/*@=sizeoftype@*/

/**
 */
static void mtprngReload(mtprngParam* mp)
	/*@modifies mp @*/
{
    register uint32* p0 = mp->state;
    register uint32* p2=p0+2, *pM = p0+M, s0, s1;
    register int j;

    /*@-shiftsigned@*/
    for (s0=mp->state[0], s1=mp->state[1], j=N-M+1; --j; s0=s1, s1=*(p2++))
        *(p0++) = *(pM++) ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    for (pM=mp->state, j=M; --j; s0=s1, s1=*(p2++))
        *(p0++) = *(pM++) ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    s1 = mp->state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);
    /*@=shiftsigned@*/

    mp->left = N;
    mp->nextw = mp->state;
}

int mtprngSetup(mtprngParam* mp)
{
	if (mp)
	{
		#ifdef _REENTRANT
		# if WIN32
		if (!(mp->lock = CreateMutex(NULL, FALSE, NULL)))
			return -1;
		# else
		#  if defined(HAVE_SYNCH_H)
		if (mutex_init(&mp->lock, USYNC_THREAD, (void *) 0))
			return -1;
		#  elif defined(HAVE_PTHREAD_H)
		/*@-nullpass@*/
		/*@-moduncon@*/
		if (pthread_mutex_init(&mp->lock, (pthread_mutexattr_t *) 0))
			return -1;
		/*@=moduncon@*/
		/*@=nullpass@*/
		#  endif
		# endif
		#endif

		mp->left = 0;

		return entropyGatherNext(mp->state, N+1);
	}
	return -1;
}

int mtprngSeed(mtprngParam* mp, const uint32* data, int size)
{
	if (mp)
	{
		int	needed = N+1;
		uint32*	dest = mp->state;

		#ifdef _REENTRANT
		# if WIN32
		if (WaitForSingleObject(mp->lock, INFINITE) != WAIT_OBJECT_0)
			return -1;
		# else
		#  if defined(HAVE_SYNCH_H)
		if (mutex_lock(&mp->lock))
			return -1;
		#  elif defined(HAVE_PTHREAD_H)
		/*@-moduncon@*/
		if (pthread_mutex_lock(&mp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif
		while (size < needed)
		{
			mp32copy(size, dest, data);
			dest += size;
			needed -= size;
		}
		mp32copy(needed, dest, data);
		#ifdef _REENTRANT
		# if WIN32
		if (!ReleaseMutex(mp->lock))
			return -1;
		# else
		#  if defined(HAVE_SYNCH_H)
		if (mutex_unlock(&mp->lock))
			return -1;
		#  elif defined(HAVE_PTHREAD_H)
		/*@-moduncon@*/
		if (pthread_mutex_unlock(&mp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

int mtprngNext(mtprngParam* mp, uint32* data, int size)
{
	if (mp)
	{
		register uint32 tmp;

		#ifdef _REENTRANT
		# if WIN32
		if (WaitForSingleObject(mp->lock, INFINITE) != WAIT_OBJECT_0)
			return -1;
		# else
		#  if defined(HAVE_SYNCH_H)
		if (mutex_lock(&mp->lock))
			return -1;
		#  elif defined(HAVE_PTHREAD_H)
		/*@-moduncon@*/
		if (pthread_mutex_lock(&mp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif
		/*@-branchstate@*/
		while (size--)
		{
			if (mp->left == 0)
				mtprngReload(mp);

			tmp = *(mp->nextw++);
			tmp ^= (tmp >> 11);
			tmp ^= (tmp << 7) & 0x9D2C5680;
			tmp ^= (tmp << 15) & 0xEFC60000;
			tmp ^= (tmp >> 18);
			mp->left--;
			*(data++) = tmp;
		}
		/*@=branchstate@*/
		#ifdef _REENTRANT
		# if WIN32
		if (!ReleaseMutex(mp->lock))
			return -1;
		# else
		#  if defined(HAVE_SYNCH_H)
		if (mutex_unlock(&mp->lock))
			return -1;
		#  elif defined(HAVE_PTHREAD_H)
		/*@-moduncon@*/
		if (pthread_mutex_unlock(&mp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

int mtprngCleanup(mtprngParam* mp)
{
	if (mp)
	{
		#ifdef _REENTRANT
		# if WIN32
		if (!CloseHandle(mp->lock))
			return -1;
		# else
		#  if defined(HAVE_SYNCH_H)
		if (mutex_destroy(&mp->lock))
			return -1;
		#  elif defined(HAVE_PTHREAD_H)
		/*@-moduncon@*/
		if (pthread_mutex_destroy(&mp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}
