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

/*!\file Security.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_SECURITY_H
#define _CLASS_SECURITY_H

#ifdef __cplusplus

#include "beecrypt/c++/mutex.h"
using beecrypt::mutex;
#include "beecrypt/c++/util/Properties.h"
using beecrypt::util::Properties;
#include "beecrypt/c++/security/Provider.h"
using beecrypt::security::Provider;
#include "beecrypt/c++/security/NoSuchAlgorithmException.h"
using beecrypt::security::NoSuchAlgorithmException;
#include "beecrypt/c++/security/NoSuchProviderException.h"
using beecrypt::security::NoSuchProviderException;
#include "beecrypt/c++/security/cert/CertificateFactory.h"
using beecrypt::security::cert::CertificateFactory;
#include "beecrypt/c++/crypto/Mac.h"
using beecrypt::crypto::Mac;
#include "beecrypt/c++/crypto/SecretKeyFactory.h"
using beecrypt::crypto::SecretKeyFactory;

#include <vector>
using std::vector;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI Security
		{
			friend class AlgorithmParameterGenerator;
			friend class AlgorithmParameters;
			friend class CertificateFactory;
			friend class KeyFactory;
			friend class KeyPairGenerator;
			friend class KeyStore;
			friend class Mac;
			friend class MessageDigest;
			friend class SecretKeyFactory;
			friend class SecureRandom;
			friend class Signature;

			public:
				typedef vector<const Provider*> provider_vector;
				typedef provider_vector::iterator provider_vector_iterator;

			private:
				struct spi
				{
					void* cspi;
					String name;
					const Provider& prov;

					spi(void* cspi, const String&, const Provider&);
				};

				static spi* getSpi(const String& name, const String& type) throw (NoSuchAlgorithmException);
				static spi* getSpi(const String& algo, const String& type, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException);
				static spi* getSpi(const String& algo, const String& type, const Provider&) throw (NoSuchAlgorithmException);
				static spi* getFirstSpi(const String& type);

				static const String& getKeyStoreDefault();

				static bool _init;
				static Properties _props;
				static mutex _lock;
				static provider_vector _providers;

				static void initialize();
				
			public:
				static int addProvider(const Provider& provider);
				static int insertProviderAt(const Provider& provider, size_t position);
				static void removeProvider(const String& name);
				static const Provider* getProvider(const String& name);
				static const provider_vector& getProviders();
		};
	}
}

#endif

#endif
