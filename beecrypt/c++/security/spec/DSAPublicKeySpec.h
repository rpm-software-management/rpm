/*
 * Copyright (c) 2004 Beeyond Software Holding BV
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
 */

/*!\file DSAPublicKeySpec.h
 * \ingroup CXX_SECURITY_SPEC_m
 */

#ifndef _CLASS_DSAPUBLICKEYSPEC_H
#define _CLASS_DSAPUBLICKEYSPEC_H

#include "beecrypt/api.h"
#include "beecrypt/mpbarrett.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/spec/KeySpec.h"
using beecrypt::security::spec::KeySpec;

namespace beecrypt {
	namespace security {
		namespace spec {
			class BEECRYPTCXXAPI DSAPublicKeySpec : public KeySpec
			{
				private:
					mpbarrett _p;
					mpbarrett _q;
					mpnumber _g;
					mpnumber _y;

				public:
					DSAPublicKeySpec(const mpbarrett& p, const mpbarrett& q, const mpnumber& g, const mpnumber& y);
					virtual ~DSAPublicKeySpec();

					const mpbarrett& getP() const throw ();
					const mpbarrett& getQ() const throw ();
					const mpnumber& getG() const throw ();
					const mpnumber& getY() const throw ();
			};
		}
	}
}

#endif

#endif
