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

/*!\file PKCS12PBEKey.h
 * \ingroup CXX_BEEYOND_m
 */

#ifndef _CLASS_PKCS12PBEKEY_H
#define _CLASS_PKCS12PBEKEY_H

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::array;
using beecrypt::bytearray;
#include "beecrypt/c++/crypto/interfaces/PBEKey.h"
using beecrypt::crypto::interfaces::PBEKey;

namespace beecrypt {
	namespace beeyond {
		class BEECRYPTCXXAPI PKCS12PBEKey : public PBEKey
		{
			private:
				array<javachar>    _pswd;
				bytearray*         _salt;
				size_t             _iter;
				mutable bytearray* _enc;

			public:
				static bytearray* encode(const array<javachar>&, const bytearray*, size_t);

			public:
				PKCS12PBEKey(const array<javachar>&, const bytearray*, size_t);
				virtual ~PKCS12PBEKey();

				virtual PKCS12PBEKey* clone() const;

				virtual size_t getIterationCount() const throw ();
				virtual const array<javachar>& getPassword() const throw ();
				virtual const bytearray* getSalt() const throw ();

				virtual const bytearray* getEncoded() const;

				virtual const String& getAlgorithm() const throw();
				virtual const String* getFormat() const throw ();
		};
	}
}

#endif

#endif
