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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/crypto/spec/DHPrivateKeySpec.h"
using beecrypt::crypto::spec::DHPrivateKeySpec;
#include "beecrypt/c++/crypto/spec/DHPublicKeySpec.h"
using beecrypt::crypto::spec::DHPublicKeySpec;
#include "beecrypt/c++/provider/DHKeyFactory.h"
#include "beecrypt/c++/provider/DHPrivateKeyImpl.h"
#include "beecrypt/c++/provider/DHPublicKeyImpl.h"
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;
#include "beecrypt/c++/security/spec/EncodedKeySpec.h"
using beecrypt::security::spec::EncodedKeySpec;

using namespace beecrypt::provider;

DHKeyFactory::DHKeyFactory()
{
}

DHKeyFactory::~DHKeyFactory()
{
}

PrivateKey* DHKeyFactory::engineGeneratePrivate(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const DHPrivateKeySpec* dh = dynamic_cast<const DHPrivateKeySpec*>(&spec);
	if (dh)
	{
		return new DHPrivateKeyImpl(dh->getP(), dh->getG(), dh->getX());
	}

	const EncodedKeySpec* enc = dynamic_cast<const EncodedKeySpec*>(&spec);
	if (enc)
	{
		try
		{
			KeyFactory* kf = KeyFactory::getInstance(enc->getFormat());
			try
			{
				PrivateKey* pri = kf->generatePrivate(*enc);
				delete kf;
				return pri;
			}
			catch (...)
			{
				delete kf;
				throw;
			}
		}
		catch (NoSuchAlgorithmException)
		{
			throw InvalidKeySpecException("Unsupported KeySpec encoding format");
		}
	}
	throw InvalidKeySpecException("Unsupported KeySpec type");
}

PublicKey* DHKeyFactory::engineGeneratePublic(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const DHPublicKeySpec* dh = dynamic_cast<const DHPublicKeySpec*>(&spec);
	if (dh)
	{
		return new DHPublicKeyImpl(dh->getP(), dh->getG(), dh->getY());
	}

	const EncodedKeySpec* enc = dynamic_cast<const EncodedKeySpec*>(&spec);
	if (enc)
	{
		try
		{
			KeyFactory* kf = KeyFactory::getInstance(enc->getFormat());
			try
			{
				PublicKey* pub = kf->generatePublic(*enc);
				delete kf;
				return pub;
			}
			catch (...)
			{
				delete kf;
				throw;
			}
		}
		catch (NoSuchAlgorithmException)
		{
			throw InvalidKeySpecException("Unsupported KeySpec encoding format");
		}
	}
	throw InvalidKeySpecException("Unsupported KeySpec type");
}

KeySpec* DHKeyFactory::engineGetKeySpec(const Key& key, const type_info& info) throw (InvalidKeySpecException)
{
	const DHPublicKey* pub = dynamic_cast<const DHPublicKey*>(&key);
	if (pub)
	{
		if (info == typeid(KeySpec) || info == typeid(DHPrivateKeySpec))
		{
			const DHParams& params = pub->getParams();

			return new DHPublicKeySpec(params.getP(), params.getG(), pub->getY());
		}
		/*!\todo also support EncodedKeySpec
		 */
		/*
		if (info == typeid(EncodedKeySpec))
		{
		}
		*/

		throw InvalidKeySpecException("Unsupported KeySpec type");
	}

	const DHPrivateKey* pri = dynamic_cast<const DHPrivateKey*>(&key);
	if (pri)
	{
		if (info == typeid(KeySpec) || info == typeid(DHPublicKeySpec))
		{
			const DHParams& params = pri->getParams();

			return new DHPrivateKeySpec(params.getP(), params.getG(), pri->getX());
		}
		/*!\todo also support EncodedKeySpec
		 */
		/*
		if (info == typeid(EncodedKeySpec))
		{
		}
		*/

		throw InvalidKeySpecException("Unsupported KeySpec type");
	}

	throw InvalidKeySpecException("Unsupported Key type");
}

Key* DHKeyFactory::engineTranslateKey(const Key& key) throw (InvalidKeyException)
{
	const DHPublicKey* pub = dynamic_cast<const DHPublicKey*>(&key);
	if (pub)
		return new DHPublicKeyImpl(*pub);

	const DHPrivateKey* pri = dynamic_cast<const DHPrivateKey*>(&key);
	if (pri)
		return new DHPrivateKeyImpl(*pri);

	throw InvalidKeyException("Unsupported Key type");
}
