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

/*!\file SecureRandom.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_SECURERANDOM_H
#define _CLASS_SECURERANDOM_H

#include "beecrypt/beecrypt.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/SecureRandomSpi.h"
using beecrypt::security::SecureRandomSpi;
#include "beecrypt/c++/security/Provider.h"
using beecrypt::security::Provider;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI SecureRandom
		{
			public:
				static SecureRandom* getInstance(const String&) throw (NoSuchAlgorithmException);
				static SecureRandom* getInstance(const String&, const String&) throw (NoSuchAlgorithmException, NoSuchProviderException);
				static SecureRandom* getInstance(const String&, const Provider&) throw (NoSuchAlgorithmException);

				static void getSeed(byte*, size_t);

			private:
				SecureRandomSpi* _rspi;
				String           _type;
				const Provider*  _prov;

			protected:
				SecureRandom(SecureRandomSpi*, const String&, const Provider&);

			public:
				SecureRandom();
				~SecureRandom();

				void generateSeed(byte*, size_t);
				void nextBytes(byte*, size_t);
				void setSeed(const byte*, size_t);

				const String& getType() const throw ();
				const Provider& getProvider() const throw ();
		};
	}
}

#endif

#endif
