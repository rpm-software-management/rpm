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

/*!\file DSAPublicKeyImpl.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_DSAPUBLICKEYIMPL_H
#define _CLASS_DSAPUBLICKEYIMPL_H

#ifdef __cplusplus

#include "beecrypt/c++/security/interfaces/DSAPublicKey.h"
using beecrypt::security::interfaces::DSAPublicKey;
#include "beecrypt/c++/security/spec/DSAParameterSpec.h"
using beecrypt::security::spec::DSAParameterSpec;

namespace beecrypt {
	namespace provider {
		class DSAPublicKeyImpl : public DSAPublicKey
		{
			private:
				DSAParameterSpec* _params;
				mpnumber _y;
				mutable bytearray* _enc;

			public:
				DSAPublicKeyImpl(const DSAPublicKey&);
				DSAPublicKeyImpl(const DSAParams&, const mpnumber&);
				DSAPublicKeyImpl(const dsaparam&, const mpnumber&);
				DSAPublicKeyImpl(const mpbarrett&, const mpbarrett&, const mpnumber&, const mpnumber&);
				virtual ~DSAPublicKeyImpl();

				virtual DSAPublicKey* clone() const;

				virtual const DSAParams& getParams() const throw ();
				virtual const mpnumber& getY() const throw ();

				virtual const bytearray* getEncoded() const;
				virtual const String& getAlgorithm() const throw ();
				virtual const String* getFormat() const throw ();
		};
	}
}

#endif

#endif
