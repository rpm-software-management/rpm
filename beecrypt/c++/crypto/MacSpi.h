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

/*!\file MacSpi.h
 * \ingroup CXX_CRYPTO_m
 */

#ifndef _CLASS_MACSPI_H
#define _CLASS_MACSPI_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::array;
#include "beecrypt/c++/lang/IllegalStateException.h"
using beecrypt::lang::IllegalStateException;
#include "beecrypt/c++/security/InvalidAlgorithmParameterException.h"
using beecrypt::security::InvalidAlgorithmParameterException;
#include "beecrypt/c++/security/InvalidKeyException.h"
using beecrypt::security::InvalidKeyException;
#include "beecrypt/c++/security/Key.h"
using beecrypt::security::Key;
#include "beecrypt/c++/security/ShortBufferException.h"
using beecrypt::security::ShortBufferException;
#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace crypto {
		class BEECRYPTCXXAPI MacSpi
		{
			friend class Mac;

			protected:
				virtual const bytearray& engineDoFinal() = 0;
				virtual size_t engineDoFinal(byte*, size_t, size_t) throw (ShortBufferException) = 0;
				virtual size_t engineGetMacLength() = 0;
				virtual void engineInit(const Key&, const AlgorithmParameterSpec*) throw (InvalidKeyException, InvalidAlgorithmParameterException) = 0; 
				virtual void engineReset() = 0;
				virtual void engineUpdate(byte) = 0;
				virtual void engineUpdate(const byte*, size_t, size_t) = 0;

			public:
				virtual ~MacSpi() {};

				virtual MacSpi* clone() const = 0;
		};
	}
}

#endif

#endif
