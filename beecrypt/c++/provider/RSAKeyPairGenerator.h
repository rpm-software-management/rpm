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

/*!\file RSAKeyPairGenerator.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_RSAKEYPAIRGENERATOR_H
#define _CLASS_RSAKEYPAIRGENERATOR_H

#ifdef __cplusplus

#include "beecrypt/c++/security/KeyPairGeneratorSpi.h"
using beecrypt::security::KeyPairGeneratorSpi;
using beecrypt::security::KeyPair;
#include "beecrypt/c++/security/SecureRandom.h"
using beecrypt::security::SecureRandom;
#include "beecrypt/c++/security/spec/RSAKeyGenParameterSpec.h"
using beecrypt::security::spec::RSAKeyGenParameterSpec;

using beecrypt::security::InvalidAlgorithmParameterException;
using beecrypt::security::InvalidParameterException;

namespace beecrypt {
	namespace provider {
		class RSAKeyPairGenerator : public KeyPairGeneratorSpi
		{
			private:
				size_t _size;
				RSAKeyGenParameterSpec* _spec;
				SecureRandom* _srng;

				KeyPair* genpair(randomGeneratorContext*);

			protected:
				virtual KeyPair* engineGenerateKeyPair();

				virtual void engineInitialize(const AlgorithmParameterSpec&, SecureRandom*) throw (InvalidAlgorithmParameterException);
				virtual void engineInitialize(size_t, SecureRandom*) throw (InvalidParameterException);

			public:
				RSAKeyPairGenerator();
				virtual ~RSAKeyPairGenerator();
		};
	}
}

#endif

#endif
