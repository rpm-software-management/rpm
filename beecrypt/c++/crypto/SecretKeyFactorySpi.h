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

/*!\file SecretKeyFactorySpi.h
 * \ingroup CXX_CRYPTO_m
 */

#ifndef _CLASS_SECRETKEYFACTORYSPI_H
#define _CLASS_SECRETKEYFACTORYSPI_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/crypto/SecretKey.h"
using beecrypt::crypto::SecretKey;
#include "beecrypt/c++/security/InvalidKeyException.h"
using beecrypt::security::InvalidKeyException;
#include "beecrypt/c++/security/spec/KeySpec.h"
using beecrypt::security::spec::KeySpec;
#include "beecrypt/c++/security/spec/InvalidKeySpecException.h"
using beecrypt::security::spec::InvalidKeySpecException;

#include <typeinfo>
using std::type_info;

namespace beecrypt {
	namespace crypto {
		class BEECRYPTCXXAPI SecretKeyFactorySpi
		{
			friend class SecretKeyFactory;

			protected:
				virtual SecretKey* engineGenerateSecret(const KeySpec&) throw (InvalidKeySpecException) = 0;
				virtual KeySpec* engineGetKeySpec(const SecretKey&, const type_info&) throw (InvalidKeySpecException) = 0;
				virtual SecretKey* engineTranslateKey(const SecretKey&) throw (InvalidKeyException) = 0;

			public:
				virtual ~SecretKeyFactorySpi() {};
		};
	}
}

#endif

#endif
