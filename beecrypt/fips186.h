/*
 * Copyright (c) 1998, 1999, 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file fips186.h
 * \brief FIPS-186 pseudo-random number generator, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup PRNG_m PRNG_fips186_m
 */

#ifndef _FIPS186_H
#define _FIPS186_H

#include "beecrypt.h"
#include "sha1.h"

#if (MP_WBITS == 64)
# define FIPS186_STATE_SIZE	8
#elif (MP_WBITS == 32)
# define FIPS186_STATE_SIZE	16
#else
# error
#endif

/*!\ingroup PRNG_fips186_m
 */
typedef struct
{
	#ifdef _REENTRANT
	# if WIN32
	HANDLE		lock;
	# else
	bc_lock_t	lock;
	# endif
	#endif
	sha1Param	param;
	mpw		state[FIPS186_STATE_SIZE];
	byte		digest[20];
	unsigned char	digestremain;
} fips186Param;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
/*@observer@*/ /*@unchecked@*/
extern BEECRYPTAPI const randomGenerator fips186prng;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Setup  (fips186Param* fp)
	/*@modifies fp @*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Seed   (fips186Param* fp, const byte* data, size_t size)
	/*@modifies fp @*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Next   (fips186Param* fp, byte* data, size_t size)
	/*@modifies fp, data @*/;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
int fips186Cleanup(fips186Param* fp)
	/*@modifies fp @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
