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

#ifndef _CLASS_KEYPROTECTOR_H
#define _CLASS_KEYPROTECTOR_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/crypto/interfaces/PBEKey.h"
using beecrypt::crypto::interfaces::PBEKey;
#include "beecrypt/c++/security/PrivateKey.h"
using beecrypt::security::PrivateKey;
#include "beecrypt/c++/security/InvalidKeyException.h"
using beecrypt::security::InvalidKeyException;
#include "beecrypt/c++/security/UnrecoverableKeyException.h"
using beecrypt::security::UnrecoverableKeyException;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;

namespace beecrypt {
	namespace provider {
		class KeyProtector
		{
		private:
			byte _cipher_key[32];
			byte _mac_key[32];
			byte _iv[16];

		public:
			KeyProtector(PBEKey&) throw (InvalidKeyException);
			~KeyProtector() throw ();

			bytearray* protect(const PrivateKey&) throw ();

			PrivateKey* recover(const bytearray&) throw (NoSuchAlgorithmException, UnrecoverableKeyException);
			PrivateKey* recover(const byte*, size_t) throw (NoSuchAlgorithmException, UnrecoverableKeyException);
		};
	}
}

#endif

#endif
