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

/*!\file RSAPrivateCrtKeySpec.h
 * \ingroup CXX_SECURITY_SPEC_m
 */

#ifndef _CLASS_RSAPRIVATECRTKEYSPEC_H
#define _CLASS_RSAPRIVATECRTKEYSPEC_H

#ifdef __cplusplus

#include "beecrypt/c++/security/spec/RSAPrivateKeySpec.h"
using beecrypt::security::spec::RSAPrivateKeySpec;

namespace beecrypt {
	namespace security {
		namespace spec {
			class BEECRYPTCXXAPI RSAPrivateCrtKeySpec : public RSAPrivateKeySpec
			{
				private:
					mpnumber _e;
					mpbarrett _p;
					mpbarrett _q;
					mpnumber _dp;
					mpnumber _dq;
					mpnumber _qi;

				public:
					RSAPrivateCrtKeySpec(const mpbarrett& modulus, const mpnumber& publicExponent, const mpnumber& privateExponent, const mpbarrett& primeP, const mpbarrett& primeQ, const mpnumber& primeExponentP, const mpnumber& primeExponentQ, const mpnumber& crtCoefficient);
					virtual ~RSAPrivateCrtKeySpec();

					const mpnumber& getPublicExponent() const throw ();
					const mpbarrett& getPrimeP() const throw ();
					const mpbarrett& getPrimeQ() const throw ();
					const mpnumber& getPrimeExponentP() const throw ();
					const mpnumber& getPrimeExponentQ() const throw ();
					const mpnumber& getCrtCoefficient() const throw ();
			};
		}
	}
}

#endif

#endif
