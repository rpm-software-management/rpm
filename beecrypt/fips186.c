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

#include "system.h"
#include "beecrypt.h"
#include "endianness.h"		/* XXX for encodeInts */
#include "fips186.h"
#include "mpopt.h"
#include "mp.h"
#include "debug.h"

/*!\addtogroup PRNG_fips186_m
 * \{
 */

/**
 */
/*@observer@*/ /*@unchecked@*/
static uint32_t fips186hinit[5] = { 0xefcdab89U, 0x98badcfeU, 0x10325476U, 0xc3d2e1f0U, 0x67452301U };

/*@-sizeoftype@*/
const randomGenerator fips186prng = { "FIPS 186", sizeof(fips186Param), (const randomGeneratorSetup) fips186Setup, (const randomGeneratorSeed) fips186Seed, (const randomGeneratorNext) fips186Next, (const randomGeneratorCleanup) fips186Cleanup };
/*@=sizeoftype@*/

/**
 */
static int fips186init(register sha1Param* p)
	/*@modifies p @*/
{
	memcpy(p->h, fips186hinit, sizeof(p->h));
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
		/*@-nullpass@*/
		/*@-moduncon@*/
		if (pthread_mutex_init(&fp->lock, (pthread_mutexattr_t *) 0))
			return -1;
		/*@=moduncon@*/
		/*@=nullpass@*/
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
		/*@-moduncon@*/
		if (pthread_mutex_lock(&fp->lock))
			return -1;
		/*@=moduncon@*/
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
				(void) mpadd(FIPS186_STATE_SIZE, fp->state, seed);
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
		/*@-moduncon@*/
		if (pthread_mutex_unlock(&fp->lock))
			return -1;
		/*@=moduncon@*/
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
		/*@-moduncon@*/
		if (pthread_mutex_lock(&fp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif

		while (size > 0)
		{
			register size_t copy;

			if (fp->digestremain == 0)
			{
				(void) fips186init(&fp->param);
				/* copy the 512 bits of state data into the sha1Param */
				memcpy(fp->param.data, fp->state, MP_WORDS_TO_BYTES(FIPS186_STATE_SIZE));
				/* process the data */
				sha1Process(&fp->param);
				(void) encodeInts(fp->param.h, fp->digest, 5);
				if (os2ip(dig, FIPS186_STATE_SIZE, fp->digest, 20) == 0)
				{
					/* set state to state + digest + 1 mod 2^512 */
					(void) mpadd (FIPS186_STATE_SIZE, fp->state, dig);
					(void) mpaddw(FIPS186_STATE_SIZE, fp->state, 1);
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
		/*@-moduncon@*/
		if (pthread_mutex_unlock(&fp->lock))
			return -1;
		/*@=moduncon@*/
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
		/*@-moduncon@*/
		if (pthread_mutex_destroy(&fp->lock))
			return -1;
		/*@=moduncon@*/
		#  endif
		# endif
		#endif
		return 0;
	}
	return -1;
}

/*!\}
 */
