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

/*!\file PKCS1RSASignature.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_PKCS1RSASIGNATURE_H
#define _CLASS_PKCS1RSASIGNATURE_H

#include "beecrypt/api.h"
#include "beecrypt/rsa.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/SignatureSpi.h"
using beecrypt::security::SecureRandom;
using beecrypt::security::SignatureSpi;
using beecrypt::security::AlgorithmParameters;
using beecrypt::security::InvalidAlgorithmParameterException;
using beecrypt::security::InvalidKeyException;
using beecrypt::security::PrivateKey;
using beecrypt::security::PublicKey;
using beecrypt::security::ShortBufferException;
using beecrypt::security::SignatureException;
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace provider {
		class PKCS1RSASignature : public SignatureSpi
		{
			private:
				rsakp _pair;
				bool _crt;
				hashFunctionContext _hfc;
				SecureRandom* _srng;

			protected:
				PKCS1RSASignature(const hashFunction*);

				virtual AlgorithmParameters* engineGetParameters() const;
				virtual void engineSetParameter(const AlgorithmParameterSpec&) throw (InvalidAlgorithmParameterException);

				virtual void engineInitSign(const PrivateKey&, SecureRandom*) throw (InvalidKeyException);
				virtual void engineInitVerify(const PublicKey&) throw (InvalidKeyException);

				virtual bytearray* engineSign() throw (SignatureException);
				virtual size_t engineSign(byte*, size_t, size_t) throw (ShortBufferException, SignatureException);
				virtual size_t engineSign(bytearray&) throw (SignatureException);
				virtual bool engineVerify(const byte*, size_t, size_t) throw (SignatureException);

				virtual void engineUpdate(byte);
				virtual void engineUpdate(const byte*, size_t, size_t);

			public:
				virtual ~PKCS1RSASignature();
		};
	}
}

#endif

#endif
