/*
 * Copyright (c) 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

/*!\mtprng.c
 * \brief Mersenne Twister pseudo-random number generator.
 *
 * Developed by Makoto Matsumoto and Takuji Nishimura. For more information,
 * see: http://www.math.keio.ac.jp/~matumoto/emt.html
 *
 * Adapted from optimized code by Shawn J. Cokus <cokus@math.washington.edu>
 *
 * \warning This generator has a very long period, passes statistical test and
 *          is very fast, but is not recommended for use in cryptography.
 * 
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup PRNG_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/mtprng.h"

#define hiBit(a)		((a) & 0x80000000U)
#define loBit(a)		((a) & 0x1U)
#define loBits(a)		((a) & 0x7FFFFFFFU)
#define mixBits(a, b)	(hiBit(a) | loBits(b))

const randomGenerator mtprng = { "Mersenne Twister", sizeof(mtprngParam), (randomGeneratorSetup) mtprngSetup, (randomGeneratorSeed) mtprngSeed, (randomGeneratorNext) mtprngNext, (randomGeneratorCleanup) mtprngCleanup };

static void mtprngReload(mtprngParam* mp)
{
    register uint32_t *p0 = mp->state;
	register uint32_t *p2 = p0+2, *pM = p0+M, s0, s1;
    register int j;

    for (s0=mp->state[0], s1=mp->state[1], j=N-M+1; --j; s0=s1, s1=*(p2++))
        *(p0++) = *(pM++) ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    for (pM=mp->state, j=M; --j; s0=s1, s1=*(p2++))
        *(p0++) = *(pM++) ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

    s1 = mp->state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0);

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
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_init(&mp->lock, USYNC_THREAD, (void *) 0))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_init(&mp->lock, (pthread_mutexattr_t *) 0))
			return -1;
		#  endif
		# endif
		#endif

		mp->left = 0;

		return entropyGatherNext((byte*) mp->state, (N+1) * sizeof(uint32_t));
	}
	return -1;
}

int mtprngSeed(mtprngParam* mp, const byte* data, size_t size)
{
	if (mp)
	{
		size_t	needed = (N+1) * sizeof(uint32_t);
		byte*	dest = (byte*) mp->state;

		#ifdef _REENTRANT
		# if WIN32
		if (WaitForSingleObject(mp->lock, INFINITE) != WAIT_OBJECT_0)
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_lock(&mp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_lock(&mp->lock))
			return -1;
		#  endif
		# endif
		#endif
		while (size < needed)
		{
			memcpy(dest, data, size);
			dest += size;
			needed -= size;
		}
		memcpy(dest, data, needed);
		#ifdef _REENTRANT
		# if WIN32
		if (!ReleaseMutex(mp->lock))
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_unlock(&mp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_unlock(&mp->lock))
			return -1;
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

int mtprngNext(mtprngParam* mp, byte* data, size_t size)
{
	if (mp)
	{
		uint32_t tmp;

		#ifdef _REENTRANT
		# if WIN32
		if (WaitForSingleObject(mp->lock, INFINITE) != WAIT_OBJECT_0)
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_lock(&mp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_lock(&mp->lock))
			return -1;
		#  endif
		# endif
		#endif
		while (size > 0)
		{
			if (mp->left == 0)
				mtprngReload(mp);

			tmp = *(mp->nextw++);
			tmp ^= (tmp >> 11);
			tmp ^= (tmp << 7) & 0x9D2C5680U;
			tmp ^= (tmp << 15) & 0xEFC60000U;
			tmp ^= (tmp >> 18);
			mp->left--;

			if (size >= 4)
			{
				memcpy(data, &tmp, 4);
				size -= 4;
			}
			else
			{
				memcpy(data, &tmp, size);
				size = 0;
			}
		}
		#ifdef _REENTRANT
		# if WIN32
		if (!ReleaseMutex(mp->lock))
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_unlock(&mp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_unlock(&mp->lock))
			return -1;
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
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_destroy(&mp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_destroy(&mp->lock))
			return -1;
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}
