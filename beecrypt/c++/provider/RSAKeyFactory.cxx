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

#include "beecrypt/c++/provider/RSAKeyFactory.h"
#include "beecrypt/c++/provider/RSAPrivateKeyImpl.h"
#include "beecrypt/c++/provider/RSAPrivateCrtKeyImpl.h"
#include "beecrypt/c++/provider/RSAPublicKeyImpl.h"
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;
#include "beecrypt/c++/security/spec/EncodedKeySpec.h"
using beecrypt::security::spec::EncodedKeySpec;
#include "beecrypt/c++/security/spec/RSAPrivateKeySpec.h"
using beecrypt::security::spec::RSAPrivateKeySpec;
#include "beecrypt/c++/security/spec/RSAPrivateCrtKeySpec.h"
using beecrypt::security::spec::RSAPrivateCrtKeySpec;
#include "beecrypt/c++/security/spec/RSAPublicKeySpec.h"
using beecrypt::security::spec::RSAPublicKeySpec;

using beecrypt::security::NoSuchAlgorithmException;

using namespace beecrypt::provider;

RSAKeyFactory::RSAKeyFactory()
{
}

RSAKeyFactory::~RSAKeyFactory()
{
}

PrivateKey* RSAKeyFactory::engineGeneratePrivate(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const RSAPrivateKeySpec* rsa = dynamic_cast<const RSAPrivateKeySpec*>(&spec);
	if (rsa)
	{
		const RSAPrivateCrtKeySpec* crt = dynamic_cast<const RSAPrivateCrtKeySpec*>(rsa);
		if (crt)
			return new RSAPrivateCrtKeyImpl(crt->getModulus(), crt->getPublicExponent(), crt->getPrivateExponent(), crt->getPrimeP(), crt->getPrimeQ(), crt->getPrimeExponentP(), crt->getPrimeExponentQ(), crt->getCrtCoefficient());
		else
			return new RSAPrivateKeyImpl(rsa->getModulus(), rsa->getPrivateExponent());
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

PublicKey* RSAKeyFactory::engineGeneratePublic(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const RSAPublicKeySpec* rsa = dynamic_cast<const RSAPublicKeySpec*>(&spec);

	if (rsa)
	{
		return new RSAPublicKeyImpl(rsa->getModulus(), rsa->getPublicExponent());
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

KeySpec* RSAKeyFactory::engineGetKeySpec(const Key& key, const type_info& info) throw (InvalidKeySpecException)
{
	const RSAPublicKey* pub = dynamic_cast<const RSAPublicKey*>(&key);

	if (pub)
	{
		if (info == typeid(KeySpec) || info == typeid(RSAPublicKeySpec))
		{
			return new RSAPublicKeySpec(pub->getModulus(), pub->getPublicExponent());
		}
		/* todo:
		if (info == typeid(EncodedKeySpec))
		{
		}
		*/

		throw InvalidKeySpecException("Unsupported KeySpec type");
	}

	const RSAPrivateKey* pri = dynamic_cast<const RSAPrivateKey*>(&key);

	if (pri)
	{
		const RSAPrivateCrtKey* crt = dynamic_cast<const RSAPrivateCrtKey*>(pri);

		if (crt)
		{
			if (info == typeid(KeySpec) || info == typeid(RSAPrivateCrtKeySpec))
			{
				return new RSAPrivateCrtKeySpec(crt->getModulus(), crt->getPublicExponent(), crt->getPrivateExponent(), crt->getPrimeP(), crt->getPrimeQ(), crt->getPrimeExponentP(), crt->getPrimeExponentQ(), crt->getCrtCoefficient());
			}
			/* todo:
			if (info == typeid(EncodedKeySpec))
			{
			}
			*/
		}
		else
		{
			if (info == typeid(KeySpec) || info == typeid(RSAPrivateKeySpec))
			{
				return new RSAPrivateKeySpec(pri->getModulus(), pri->getPrivateExponent());
			}
			/* todo:
			if (info == typeid(EncodedKeySpec))
			{
			}
			*/
		}

		throw InvalidKeySpecException("Unsupported KeySpec type");
	}

	throw InvalidKeySpecException("Unsupported Key type");
}

Key* RSAKeyFactory::engineTranslateKey(const Key& key) throw (InvalidKeyException)
{
	const RSAPublicKey* pub = dynamic_cast<const RSAPublicKey*>(&key);
	if (pub)
		return new RSAPublicKeyImpl(*pub);

	const RSAPrivateKey* pri = dynamic_cast<const RSAPrivateKey*>(&key);
	if (pri)
	{
		const RSAPrivateCrtKey* crt = dynamic_cast<const RSAPrivateCrtKey*>(pri);
		if (crt)
			return new RSAPrivateCrtKeyImpl(*crt);
		else
			return new RSAPrivateKeyImpl(*pri);
	}

	throw InvalidKeyException("Unsupported Key type");
}
