/*
 * Copyright (c) 1998, 1999, 2000, 2002 Virtual Unlimited B.V.
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

/*!\file fips186.c
 * \brief FIPS 186 pseudo-random number generator.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup PRNG_m PRNG_fips186_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/fips186.h"

/*!\addtogroup PRNG_fips186_m
 * \{
 */

static uint32_t fips186hinit[5] = { 0xefcdab89U, 0x98badcfeU, 0x10325476U, 0xc3d2e1f0U, 0x67452301U };

const randomGenerator fips186prng = {
	"FIPS 186",
	sizeof(fips186Param),
	(randomGeneratorSetup) fips186Setup,
	(randomGeneratorSeed) fips186Seed,
	(randomGeneratorNext) fips186Next,
	(randomGeneratorCleanup) fips186Cleanup
};

static int fips186init(register sha1Param* p)
{
	memcpy(p->h, fips186hinit, 5 * sizeof(uint32_t));
	return 0;
}

int fips186Setup(fips186Param* fp)
{
	if (fp)
	{
		#ifdef _REENTRANT
		# if WIN32
		if (!(fp->lock = CreateMutex(NULL, FALSE, NULL)))
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_init(&fp->lock, USYNC_THREAD, (void *) 0))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_init(&fp->lock, (pthread_mutexattr_t *) 0))
			return -1;
		#  endif
		# endif
		#endif

		fp->digestremain = 0;

		return entropyGatherNext((byte*) fp->state, MP_WORDS_TO_BYTES(FIPS186_STATE_SIZE));
	}
	return -1;
}

int fips186Seed(fips186Param* fp, const byte* data, size_t size)
{
	if (fp)
	{
		#ifdef _REENTRANT
		# if WIN32
		if (WaitForSingleObject(fp->lock, INFINITE) != WAIT_OBJECT_0)
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_lock(&fp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_lock(&fp->lock))
			return -1;
		#  endif
		# endif
		#endif
		if (data)
		{
			mpw seed[FIPS186_STATE_SIZE];

			/* if there's too much data, cut off at what we can deal with */
			if (size > MP_WORDS_TO_BYTES(FIPS186_STATE_SIZE))
				size = MP_WORDS_TO_BYTES(FIPS186_STATE_SIZE);

			/* convert to multi-precision integer, and add to the state */
			if (os2ip(seed, FIPS186_STATE_SIZE, data, size) == 0)
				mpadd(FIPS186_STATE_SIZE, fp->state, seed);
		}
		#ifdef _REENTRANT
		# if WIN32
		if (!ReleaseMutex(fp->lock))
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_unlock(&fp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_unlock(&fp->lock))
			return -1;
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

int fips186Next(fips186Param* fp, byte* data, size_t size)
{
	if (fp)
	{
		mpw dig[FIPS186_STATE_SIZE];

		#ifdef _REENTRANT
		# if WIN32
		if (WaitForSingleObject(fp->lock, INFINITE) != WAIT_OBJECT_0)
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_lock(&fp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_lock(&fp->lock))
			return -1;
		#  endif
		# endif
		#endif

		while (size > 0)
		{
			register size_t copy;

			if (fp->digestremain == 0)
			{
				fips186init(&fp->param);
				/* copy the 512 bits of state data into the sha1Param */
				memcpy(fp->param.data, fp->state, MP_WORDS_TO_BYTES(FIPS186_STATE_SIZE));
				/* process the data */
				sha1Process(&fp->param);
				
				#if WORDS_BIGENDIAN
				memcpy(fp->digest, fp->param.h, 20);
				#else
				/* encode 5 integers big-endian style */
				fp->digest[ 0] = (byte)(fp->param.h[0] >> 24);
				fp->digest[ 1] = (byte)(fp->param.h[0] >> 16);
				fp->digest[ 2] = (byte)(fp->param.h[0] >>  8);
				fp->digest[ 3] = (byte)(fp->param.h[0] >>  0);
				fp->digest[ 4] = (byte)(fp->param.h[1] >> 24);
				fp->digest[ 5] = (byte)(fp->param.h[1] >> 16);
				fp->digest[ 6] = (byte)(fp->param.h[1] >>  8);
				fp->digest[ 7] = (byte)(fp->param.h[1] >>  0);
				fp->digest[ 8] = (byte)(fp->param.h[2] >> 24);
				fp->digest[ 9] = (byte)(fp->param.h[2] >> 16);
				fp->digest[10] = (byte)(fp->param.h[2] >>  8);
				fp->digest[11] = (byte)(fp->param.h[2] >>  0);
				fp->digest[12] = (byte)(fp->param.h[3] >> 24);
				fp->digest[13] = (byte)(fp->param.h[3] >> 16);
				fp->digest[14] = (byte)(fp->param.h[3] >>  8);
				fp->digest[15] = (byte)(fp->param.h[3] >>  0);
				fp->digest[16] = (byte)(fp->param.h[4] >> 24);
				fp->digest[17] = (byte)(fp->param.h[4] >> 16);
				fp->digest[18] = (byte)(fp->param.h[4] >>  8);
				fp->digest[19] = (byte)(fp->param.h[4] >>  0);
				#endif

				if (os2ip(dig, FIPS186_STATE_SIZE, fp->digest, 20) == 0)
				{
					/* set state to state + digest + 1 mod 2^512 */
					mpadd (FIPS186_STATE_SIZE, fp->state, dig);
					mpaddw(FIPS186_STATE_SIZE, fp->state, 1);
				}
				/* else shouldn't occur */
				/* we now have 5 words of pseudo-random data */
				fp->digestremain = 20;
			}
			copy = (size > fp->digestremain) ? fp->digestremain : size;
			memcpy(data, fp->digest+20-fp->digestremain, copy);
			fp->digestremain -= copy;
			size -= copy;
			data += copy;
		}
		#ifdef _REENTRANT
		# if WIN32
		if (!ReleaseMutex(fp->lock))
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_unlock(&fp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_unlock(&fp->lock))
			return -1;
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

int fips186Cleanup(fips186Param* fp)
{
	if (fp)
	{
		#ifdef _REENTRANT
		# if WIN32
		if (!CloseHandle(fp->lock))
			return -1;
		# else
		#  if HAVE_THREAD_H && HAVE_SYNCH_H
		if (mutex_destroy(&fp->lock))
			return -1;
		#  elif HAVE_PTHREAD_H
		if (pthread_mutex_destroy(&fp->lock))
			return -1;
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

/*!\}
 */
