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

/*!\file PKCS12KeyFactory.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_PKCS12KEYFACTORY_H
#define _CLASS_PKCS12KEYFACTORY_H

#ifdef __cplusplus

#include "beecrypt/c++/crypto/SecretKeyFactorySpi.h"
using beecrypt::crypto::SecretKeyFactorySpi;

namespace beecrypt {
	namespace provider {
		class PKCS12KeyFactory : public SecretKeyFactorySpi
		{
			protected:
				virtual SecretKey* engineGenerateSecret(const KeySpec&) throw (InvalidKeySpecException);
				virtual KeySpec* engineGetKeySpec(const SecretKey&, const type_info&) throw (InvalidKeySpecException);
				virtual SecretKey* engineTranslateKey(const SecretKey&) throw (InvalidKeyException);

			public:
				PKCS12KeyFactory();
				virtual ~PKCS12KeyFactory();
		};
	}
}

#endif

#endif
