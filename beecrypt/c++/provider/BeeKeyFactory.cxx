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

#include "beecrypt/c++/beeyond/BeeEncodedKeySpec.h"
using beecrypt::beeyond::BeeEncodedKeySpec;
#include "beecrypt/c++/beeyond/BeeInputStream.h"
using beecrypt::beeyond::BeeInputStream;
#include "beecrypt/c++/beeyond/BeeOutputStream.h"
using beecrypt::beeyond::BeeOutputStream;
#include "beecrypt/c++/io/ByteArrayInputStream.h"
using beecrypt::io::ByteArrayInputStream;
#include "beecrypt/c++/io/ByteArrayOutputStream.h"
using beecrypt::io::ByteArrayOutputStream;
#include "beecrypt/c++/provider/BeeKeyFactory.h"
#include "beecrypt/c++/provider/DHPrivateKeyImpl.h"
#include "beecrypt/c++/provider/DHPublicKeyImpl.h"
#include "beecrypt/c++/provider/DSAPrivateKeyImpl.h"
#include "beecrypt/c++/provider/DSAPublicKeyImpl.h"
#include "beecrypt/c++/provider/RSAPrivateKeyImpl.h"
#include "beecrypt/c++/provider/RSAPrivateCrtKeyImpl.h"
#include "beecrypt/c++/provider/RSAPublicKeyImpl.h"

namespace {
	const String ALGORITHM_DH = UNICODE_STRING_SIMPLE("DH");
	const String ALGORITHM_DSA = UNICODE_STRING_SIMPLE("DSA");
	const String ALGORITHM_RSA = UNICODE_STRING_SIMPLE("RSA");
}

using namespace beecrypt::provider;

BeeKeyFactory::BeeKeyFactory()
{
}

BeeKeyFactory::~BeeKeyFactory()
{
}

PrivateKey* BeeKeyFactory::decodePrivate(const byte* data, size_t offset, size_t size)
{
	try
	{
		String algo;

		ByteArrayInputStream bis(data, offset, size);
		BeeInputStream bee(bis);

		bee.readUTF(algo);

		if (algo == ALGORITHM_DH)
		{
			mpbarrett p;
			mpnumber g;
			mpnumber x;

			bee.read(p);
			bee.read(g);
			bee.read(x);

			return new DHPrivateKeyImpl(p, g, x);
		}

		if (algo == ALGORITHM_DSA)
		{
			mpbarrett p;
			mpbarrett q;
			mpnumber g;
			mpnumber x;

			bee.read(p);
			bee.read(q);
			bee.read(g);
			bee.read(x);

			return new DSAPrivateKeyImpl(p, q, g, x);
		}

		if (algo == ALGORITHM_RSA)
		{
			mpbarrett n;
			mpnumber d;

			bee.read(n);
			bee.read(d);

			if (bee.available() > 0)
			{
				mpnumber e;
				mpbarrett p;
				mpbarrett q;
				mpnumber dp;
				mpnumber dq;
				mpnumber qi;

				bee.read(e);
				bee.read(p);
				bee.read(q);
				bee.read(dp);
				bee.read(dq);
				bee.read(qi);

				return new RSAPrivateCrtKeyImpl(n, e, d, p, q, dp, dq, qi);
			}
			return new RSAPrivateKeyImpl(n, d);
		}
	}
	catch (IOException)
	{
	}
	return 0;
}

PublicKey* BeeKeyFactory::decodePublic(const byte* data, size_t offset, size_t size)
{
	try
	{
		String algo;

		ByteArrayInputStream bis(data, offset, size);
		BeeInputStream bee(bis);

		bee.readUTF(algo);
		
		if (algo == ALGORITHM_DH)
		{
			mpbarrett p;
			mpnumber g;
			mpnumber y;

			bee.read(p);
			bee.read(g);
			bee.read(y);

			return new DHPublicKeyImpl(p, g, y);
		}

		if (algo == ALGORITHM_DSA)
		{
			mpbarrett p;
			mpbarrett q;
			mpnumber g;
			mpnumber y;

			bee.read(p);
			bee.read(q);
			bee.read(g);
			bee.read(y);

			return new DSAPublicKeyImpl(p, q, g, y);
		}

		if (algo == ALGORITHM_RSA)
		{
			mpbarrett n;
			mpnumber e;

			bee.read(n);
			bee.read(e);

			return new RSAPublicKeyImpl(n, e);
		}
	}
	catch (IOException)
	{
	}
	return 0;
}

bytearray* BeeKeyFactory::encode(const PrivateKey& pri)
{
	try
	{
		ByteArrayOutputStream bos;
		BeeOutputStream bee(bos);

		bee.writeUTF(pri.getAlgorithm());

		const DHPrivateKey* dh = dynamic_cast<const DHPrivateKey*>(&pri);
		if (dh)
		{
			bee.write(dh->getParams().getP());
			bee.write(dh->getParams().getG());
			bee.write(dh->getX());
			bee.close();

			return bos.toByteArray();
		}

		const DSAPrivateKey* dsa = dynamic_cast<const DSAPrivateKey*>(&pri);
		if (dsa)
		{
			bee.write(dsa->getParams().getP());
			bee.write(dsa->getParams().getQ());
			bee.write(dsa->getParams().getG());
			bee.write(dsa->getX());
			bee.close();

			return bos.toByteArray();
		}

		const RSAPrivateKey* rsa = dynamic_cast<const RSAPrivateKey*>(&pri);
		if (rsa)
		{
			bee.write(rsa->getModulus());

			const RSAPrivateCrtKey* crt = dynamic_cast<const RSAPrivateCrtKey*>(rsa);

			if (crt)
			{
				bee.write(crt->getPublicExponent());
				bee.write(crt->getPrivateExponent());
				bee.write(crt->getPrimeP());
				bee.write(crt->getPrimeQ());
				bee.write(crt->getPrimeExponentP());
				bee.write(crt->getPrimeExponentQ());
				bee.write(crt->getCrtCoefficient());
			}
			else
			{
				bee.write(rsa->getPrivateExponent());
			}
			bee.close();

			return bos.toByteArray();
		}
	}
	catch (IOException)
	{
	}
	return 0;
}


bytearray* BeeKeyFactory::encode(const PublicKey& pub)
{
	try
	{
		ByteArrayOutputStream bos;
		BeeOutputStream bee(bos);

		bee.writeUTF(pub.getAlgorithm());

		const DHPublicKey* dh = dynamic_cast<const DHPublicKey*>(&pub);
		if (dh)
		{
			bee.write(dh->getParams().getP());
			bee.write(dh->getParams().getG());
			bee.write(dh->getY());
			bee.close();

			return bos.toByteArray();
		}

		const DSAPublicKey* dsa = dynamic_cast<const DSAPublicKey*>(&pub);
		if (dsa)
		{
			bee.write(dsa->getParams().getP());
			bee.write(dsa->getParams().getQ());
			bee.write(dsa->getParams().getG());
			bee.write(dsa->getY());
			bee.close();

			return bos.toByteArray();
		}

		const RSAPublicKey* rsa = dynamic_cast<const RSAPublicKey*>(&pub);
		if (rsa)
		{
			bee.write(rsa->getModulus());
			bee.write(rsa->getPublicExponent());
			bee.close();

			return bos.toByteArray();
		}
	}
	catch (IOException)
	{
	}
	return 0;
}

PrivateKey* BeeKeyFactory::engineGeneratePrivate(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const EncodedKeySpec* enc = dynamic_cast<const EncodedKeySpec*>(&spec);

	if (enc && enc->getFormat().caseCompare("BEE", 0) == 0)
	{
		const bytearray& encoding = enc->getEncoded();

		PrivateKey* pri = decodePrivate(encoding.data(), 0, encoding.size());

		if (pri)
			return pri;
		else
			throw InvalidKeySpecException("Unable to decode this KeySpec to a PrivateKey");
	}
	else
		throw InvalidKeySpecException("Unsupported KeySpec");
}

PublicKey* BeeKeyFactory::engineGeneratePublic(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const EncodedKeySpec* enc = dynamic_cast<const EncodedKeySpec*>(&spec);

	if (enc && enc->getFormat().caseCompare("BEE", 0) == 0)
	{
		const bytearray& encoding = enc->getEncoded();

		PublicKey* pub = decodePublic(encoding.data(), 0, encoding.size());

		if (pub)
			return pub;
		else
			throw InvalidKeySpecException("Unable to decode this KeySpec to a PublicKey");
	}
	else
		throw InvalidKeySpecException("Unsupported KeySpec");
}

KeySpec* BeeKeyFactory::engineGetKeySpec(const Key& key, const type_info& info) throw (InvalidKeySpecException)
{
	KeySpec* result = 0;

	if (info == typeid(EncodedKeySpec))
	{
		const String* format = key.getFormat();

		if (format && format->caseCompare("BEE", 0) == 0)
		{
			result = new BeeEncodedKeySpec(*key.getEncoded());
		}
		else
		{
			bytearray* enc;

			const PublicKey* pub = dynamic_cast<const PublicKey*>(&key);

			if (pub)
			{
				enc = encode(*pub);
			}
			else
			{
				const PrivateKey* pri = dynamic_cast<const PrivateKey*>(&key);

				if (pri)
				{
					enc = encode(*pri);
				}
			}

			if (enc)
			{
				result = new BeeEncodedKeySpec(*enc);

				delete enc;
			}
		}

		if (result)
			return result;
		else
			throw InvalidKeySpecException("Unsupported key type");
	}
	else
		throw InvalidKeySpecException("Unsupported KeySpec type");
}

Key* BeeKeyFactory::engineTranslateKey(const Key&) throw (InvalidKeyException)
{
	throw InvalidKeyException("This KeyFactory can only be used for encoding and decoding");
}
