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

#include "beecrypt/c++/provider/DSAKeyFactory.h"
#include "beecrypt/c++/provider/DSAPrivateKeyImpl.h"
#include "beecrypt/c++/provider/DSAPublicKeyImpl.h"
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;
#include "beecrypt/c++/security/spec/DSAPrivateKeySpec.h"
using beecrypt::security::spec::DSAPrivateKeySpec;
#include "beecrypt/c++/security/spec/DSAPublicKeySpec.h"
using beecrypt::security::spec::DSAPublicKeySpec;
#include "beecrypt/c++/security/spec/EncodedKeySpec.h"
using beecrypt::security::spec::EncodedKeySpec;

using namespace beecrypt::provider;

DSAKeyFactory::DSAKeyFactory()
{
}

DSAKeyFactory::~DSAKeyFactory()
{
}

PrivateKey* DSAKeyFactory::engineGeneratePrivate(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const DSAPrivateKeySpec* dsa = dynamic_cast<const DSAPrivateKeySpec*>(&spec);
	if (dsa)
	{
		return new DSAPrivateKeyImpl(dsa->getP(), dsa->getQ(), dsa->getG(), dsa->getX());
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

PublicKey* DSAKeyFactory::engineGeneratePublic(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const DSAPublicKeySpec* dsa = dynamic_cast<const DSAPublicKeySpec*>(&spec);
	if (dsa)
	{
		return new DSAPublicKeyImpl(dsa->getP(), dsa->getQ(), dsa->getG(), dsa->getY());
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

KeySpec* DSAKeyFactory::engineGetKeySpec(const Key& key, const type_info& info) throw (InvalidKeySpecException)
{
	const DSAPublicKey* pub = dynamic_cast<const DSAPublicKey*>(&key);

	if (pub)
	{
		if (info == typeid(KeySpec) || info == typeid(DSAPublicKeySpec))
		{
			const DSAParams& params = pub->getParams();

			return new DSAPublicKeySpec(params.getP(), params.getQ(), params.getG(), pub->getY());
		}
		/*!\todo also support EncodeKeySpec
		 */
		/*
		if (info == typeid(EncodedKeySpec))
		{
		}
		*/

		throw InvalidKeySpecException("Unsupported KeySpec type");
	}

	const DSAPrivateKey* pri = dynamic_cast<const DSAPrivateKey*>(&key);

	if (pri)
	{
		if (info == typeid(KeySpec) || info == typeid(DSAPrivateKeySpec))
		{
			const DSAParams& params = pri->getParams();

			return new DSAPrivateKeySpec(params.getP(), params.getQ(), params.getG(), pri->getX());
		}
		/*!\todo also support EncodeKeySpec
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

Key* DSAKeyFactory::engineTranslateKey(const Key& key) throw (InvalidKeyException)
{
	const DSAPublicKey* pub = dynamic_cast<const DSAPublicKey*>(&key);
	if (pub)
		return new DSAPublicKeyImpl(*pub);

	const DSAPrivateKey* pri = dynamic_cast<const DSAPrivateKey*>(&key);
	if (pri)
		return new DSAPrivateKeyImpl(*pri);

	throw InvalidKeyException("Unsupported Key type");
}
