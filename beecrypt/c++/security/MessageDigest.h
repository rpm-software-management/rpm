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

/*!\file MessageDigest.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_MESSAGEDIGEST_H
#define _CLASS_MESSAGEDIGEST_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/MessageDigestSpi.h"
using beecrypt::security::MessageDigestSpi;
#include "beecrypt/c++/security/Provider.h"
using beecrypt::security::Provider;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI MessageDigest
		{
			public:
				static MessageDigest* getInstance(const String&) throw (NoSuchAlgorithmException);
				static MessageDigest* getInstance(const String&, const String&) throw (NoSuchAlgorithmException, NoSuchProviderException);
				static MessageDigest* getInstance(const String&, const Provider&) throw (NoSuchAlgorithmException);

			private:
				MessageDigestSpi* _mspi;
				String            _algo;
				const Provider*   _prov;

			protected:
				MessageDigest(MessageDigestSpi*, const String&, const Provider&);

			public:
				~MessageDigest();

				MessageDigest* clone() const;

				const bytearray& digest();
				const bytearray& digest(const bytearray&);
				size_t digest(byte* data, size_t offset, size_t length) throw (ShortBufferException);
				size_t getDigestLength();
				void reset();
				void update(byte);
				void update(const byte* data, size_t offset, size_t length);
				void update(const bytearray& b);

				const String& getAlgorithm() const throw ();
				const Provider& getProvider() const throw ();
		};
	}
}

#endif

#endif
