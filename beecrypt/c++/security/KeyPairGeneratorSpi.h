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

/*!\file KeyPairGeneratorSpi.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_KEYPAIRGENERATORSPI_H
#define _CLASS_KEYPAIRGENERATORSPI_H

#include "beecrypt/api.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/KeyPair.h"
using beecrypt::security::KeyPair;
#include "beecrypt/c++/security/SecureRandom.h"
using beecrypt::security::SecureRandom;
#include "beecrypt/c++/security/InvalidAlgorithmParameterException.h"
using beecrypt::security::InvalidAlgorithmParameterException;
#include "beecrypt/c++/security/InvalidParameterException.h"
using beecrypt::security::InvalidParameterException;
#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI KeyPairGeneratorSpi
		{
			friend class KeyPairGenerator;

			protected:
				virtual KeyPair* engineGenerateKeyPair() = 0;

				virtual void engineInitialize(const AlgorithmParameterSpec&, SecureRandom*) throw (InvalidAlgorithmParameterException) = 0;
				virtual void engineInitialize(size_t, SecureRandom*) throw (InvalidParameterException) = 0;

			public:
				virtual ~KeyPairGeneratorSpi() {};
		};
	}
}

#endif

#endif
