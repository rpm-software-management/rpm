/*
 * Copyright (c) 2003 Bob Deblier
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

/*!\file mpnumber.h
 * \brief Multi-precision numbers, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#ifndef _MPNUMBER_H
#define _MPNUMBER_H

#include "mp.h"

#ifdef __cplusplus
# include <iostream>
#endif

#ifdef __cplusplus
struct BEECRYPTAPI mpnumber
#else
struct _mpnumber
#endif
{
	size_t	size;
	mpw*	data;

#ifdef __cplusplus
	mpnumber();
	mpnumber(unsigned int);
	mpnumber(const mpnumber&);
	~mpnumber();

	const mpnumber& operator=(const mpnumber&);
	bool operator==(const mpnumber&);
	bool operator!=(const mpnumber&);

	void wipe();

	size_t bitlength() const;
#endif
};

#ifndef __cplusplus
typedef struct _mpnumber mpnumber;
#else
BEECRYPTAPI
std::ostream& operator<<(std::ostream&, const mpnumber&);
/*
BEECRYPTAPI
std::istream& operator>>(std::istream&, mpnumber&);
*/
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
void mpnzero(mpnumber* n)
	/*@modifies n @*/;
BEECRYPTAPI
void mpnsize(mpnumber* n, size_t size)
	/*@modifies n @*/;
BEECRYPTAPI
void mpninit(mpnumber* n, size_t size, const mpw* data)
	/*@modifies n @*/;
BEECRYPTAPI
void mpnfree(mpnumber* n)
	/*@modifies n @*/;
BEECRYPTAPI
void mpncopy(mpnumber* n, const mpnumber* copy)
	/*@modifies n @*/;
BEECRYPTAPI
void mpnwipe(mpnumber* n)
	/*@modifies n @*/;

BEECRYPTAPI
void mpnset   (mpnumber* n, size_t size, const mpw* data)
	/*@modifies n @*/;
BEECRYPTAPI
void mpnsetw  (mpnumber* n, mpw val)
	/*@modifies n @*/;

BEECRYPTAPI
int mpnsetbin(mpnumber* n, const byte* osdata, size_t ossize)
	/*@modifies n @*/;
BEECRYPTAPI
int mpnsethex(mpnumber* n, const char* hex)
	/*@modifies n @*/;

BEECRYPTAPI
int  mpninv(mpnumber* inv, const mpnumber* k, const mpnumber* mod)
	/*@modifies inv @*/;

/*!\brief Truncate the mpnumber to the specified number of (least significant) bits.
 */
BEECRYPTAPI
size_t mpntrbits(mpnumber* n, size_t bits)
	/*@modifies n @*/;
BEECRYPTAPI
size_t mpnbits(const mpnumber* n)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
