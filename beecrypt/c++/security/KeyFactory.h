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

/*!\file KeyFactory.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_KEYFACTORY_H
#define _CLASS_KEYFACTORY_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/KeyFactorySpi.h"
using beecrypt::security::KeyFactorySpi;
#include "beecrypt/c++/security/Provider.h"
using beecrypt::security::Provider;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;

#include <typeinfo>
using std::type_info;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI KeyFactory
		{
			public:
				static KeyFactory* getInstance(const String&) throw (NoSuchAlgorithmException);
				static KeyFactory* getInstance(const String&, const String&) throw (NoSuchAlgorithmException, NoSuchProviderException);
				static KeyFactory* getInstance(const String&, const Provider&) throw (NoSuchAlgorithmException);

			private:
				KeyFactorySpi*  _kspi;
				String          _algo;
				const Provider* _prov;

			protected:
				KeyFactory(KeyFactorySpi*, const String&, const Provider&);

			public:
				~KeyFactory();

				PrivateKey* generatePrivate(const KeySpec&) throw (InvalidKeySpecException);
				PublicKey* generatePublic(const KeySpec&) throw (InvalidKeySpecException);

				KeySpec* getKeySpec(const Key& key, const type_info&) throw (InvalidKeySpecException);

				Key* translateKey(const Key&) throw (InvalidKeyException);

				const String& getAlgorithm() const throw ();
				const Provider& getProvider() const throw ();
		};
	}
}

#endif

#endif
