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

/*!\file Signature.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_SIGNATURE_H
#define _CLASS_SIGNATURE_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/SignatureSpi.h"
using beecrypt::security::SignatureSpi;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI Signature
		{
			protected:
				static const int UNINITIALIZED = 0;
				static const int VERIFY = 1;
				static const int SIGN = 2;

			public:
				static Signature* getInstance(const String&) throw (NoSuchAlgorithmException);
				static Signature* getInstance(const String&, const String&) throw (NoSuchAlgorithmException, NoSuchProviderException);
				static Signature* getInstance(const String&, const Provider&) throw (NoSuchAlgorithmException);

			protected:
				int state;

			private:
				SignatureSpi*   _sspi;
				String          _algo;
				const Provider* _prov;

			protected:
				Signature(SignatureSpi*, const String&, const Provider&);

			public:
				~Signature();

				AlgorithmParameters* getParameters() const;
				void setParameter(const AlgorithmParameterSpec&) throw (InvalidAlgorithmParameterException);

				void initSign(const PrivateKey&) throw (InvalidKeyException);
				void initSign(const PrivateKey&, SecureRandom*) throw (InvalidKeyException);

				void initVerify(const PublicKey&) throw (InvalidKeyException);

				bytearray* sign() throw (IllegalStateException, SignatureException);
				size_t sign(byte*, size_t, size_t) throw (ShortBufferException, IllegalStateException, SignatureException);
				size_t sign(bytearray&) throw (IllegalStateException, SignatureException);
				bool verify(const bytearray&) throw (IllegalStateException, SignatureException);
				bool verify(const byte*, size_t, size_t) throw (IllegalStateException, SignatureException);

				void update(byte) throw (IllegalStateException);
				void update(const byte*, size_t, size_t) throw (IllegalStateException);
				void update(const bytearray&) throw (IllegalStateException);

				const String& getAlgorithm() const throw ();
				const Provider& getProvider() const throw ();
		};
	}
}

#endif

#endif
