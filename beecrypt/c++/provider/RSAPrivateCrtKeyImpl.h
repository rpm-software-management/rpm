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

/*!\file RSAPrivateCrtKeyImpl.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_RSAPRIVATECRTKEYIMPL_H
#define _CLASS_RSAPRIVATECRTKEYIMPL_H

#ifdef __cplusplus

#include "beecrypt/c++/security/interfaces/RSAPrivateCrtKey.h"
using beecrypt::security::interfaces::RSAPrivateCrtKey;

namespace beecrypt {
	namespace provider {
		class RSAPrivateCrtKeyImpl : public RSAPrivateCrtKey
		{
			private:
				mpbarrett _n;
				mpnumber _e;
				mpnumber _d;
				mpbarrett _p;
				mpbarrett _q;
				mpnumber _dp;
				mpnumber _dq;
				mpnumber _qi;
				mutable bytearray* _enc;

			public:
				RSAPrivateCrtKeyImpl(const RSAPrivateCrtKey&);
				RSAPrivateCrtKeyImpl(const mpbarrett& modulus, const mpnumber& publicExponent, const mpnumber& privateExponent, const mpbarrett& primeP, const mpbarrett& primeQ, const mpnumber& primeExponentP, const mpnumber& primeExponentQ, const mpnumber& crtCoefficient);
				virtual ~RSAPrivateCrtKeyImpl();

				virtual RSAPrivateCrtKey* clone() const;

				virtual const mpbarrett& getModulus() const throw ();
				virtual const mpnumber& getPrivateExponent() const throw ();
				virtual const mpnumber& getPublicExponent() const throw ();
				virtual const mpbarrett& getPrimeP() const throw ();
				virtual const mpbarrett& getPrimeQ() const throw ();
				virtual const mpnumber& getPrimeExponentP() const throw ();
				virtual const mpnumber& getPrimeExponentQ() const throw ();
				virtual const mpnumber& getCrtCoefficient() const throw ();

				virtual const bytearray* getEncoded() const;
				virtual const String& getAlgorithm() const throw ();
				virtual const String* getFormat() const throw ();
		};
	}
}

#endif

#endif
