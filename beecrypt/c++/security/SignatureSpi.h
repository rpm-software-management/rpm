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

/*!\file SignatureSpi.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_SIGNATURESPI_H
#define _CLASS_SIGNATURESPI_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::bytearray;
#include "beecrypt/c++/lang/IllegalStateException.h"
using beecrypt::lang::IllegalStateException;
#include "beecrypt/c++/security/AlgorithmParameters.h"
using beecrypt::security::AlgorithmParameters;
#include "beecrypt/c++/security/PrivateKey.h"
using beecrypt::security::PrivateKey;
#include "beecrypt/c++/security/PublicKey.h"
using beecrypt::security::PublicKey;
#include "beecrypt/c++/security/SecureRandom.h"
using beecrypt::security::SecureRandom;
#include "beecrypt/c++/security/InvalidAlgorithmParameterException.h"
using beecrypt::security::InvalidAlgorithmParameterException;
#include "beecrypt/c++/security/InvalidKeyException.h"
using beecrypt::security::InvalidKeyException;
#include "beecrypt/c++/security/ShortBufferException.h"
using beecrypt::security::ShortBufferException;
#include "beecrypt/c++/security/SignatureException.h"
using beecrypt::security::SignatureException;
#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI SignatureSpi
		{
			friend class Signature;

			protected:
				virtual AlgorithmParameters* engineGetParameters() const = 0;
				virtual void engineSetParameter(const AlgorithmParameterSpec&) throw (InvalidAlgorithmParameterException) = 0;

				virtual void engineInitSign(const PrivateKey&, SecureRandom*) throw (InvalidKeyException) = 0;

				virtual void engineInitVerify(const PublicKey&) = 0;

				virtual void engineUpdate(byte) = 0;
				virtual void engineUpdate(const byte*, size_t, size_t) = 0;

				virtual bytearray* engineSign() throw (SignatureException) = 0;
				virtual size_t engineSign(byte*, size_t, size_t) throw (ShortBufferException, SignatureException) = 0;
				virtual size_t engineSign(bytearray&) throw (SignatureException) = 0;
				virtual bool engineVerify(const byte*, size_t, size_t) throw (SignatureException) = 0;

			public:
				virtual ~SignatureSpi() {};
		};
	}
}

#endif

#endif
