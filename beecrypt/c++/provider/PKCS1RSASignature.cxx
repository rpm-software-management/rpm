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

#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/provider/PKCS1RSASignature.h"
#include "beecrypt/c++/security/interfaces/RSAPrivateKey.h"
using beecrypt::security::interfaces::RSAPrivateKey;
#include "beecrypt/c++/security/interfaces/RSAPrivateCrtKey.h"
using beecrypt::security::interfaces::RSAPrivateCrtKey;
#include "beecrypt/c++/security/interfaces/RSAPublicKey.h"
using beecrypt::security::interfaces::RSAPublicKey;

#include "beecrypt/pkcs1.h"

using namespace beecrypt::provider;

PKCS1RSASignature::PKCS1RSASignature(const hashFunction* hf) : _hfc(hf)
{
}

PKCS1RSASignature::~PKCS1RSASignature()
{
}

AlgorithmParameters* PKCS1RSASignature::engineGetParameters() const
{
	return 0;
}

void PKCS1RSASignature::engineSetParameter(const AlgorithmParameterSpec& spec) throw (InvalidAlgorithmParameterException)
{
	throw InvalidAlgorithmParameterException("unsupported for this algorithm");
}

void PKCS1RSASignature::engineInitSign(const PrivateKey& key, SecureRandom* random) throw (InvalidKeyException)
{
	const RSAPrivateKey* rsa = dynamic_cast<const RSAPrivateKey*>(&key);

	if (rsa)
	{
		/* copy key information */
		_pair.n = rsa->getModulus();
		_pair.d = rsa->getPrivateExponent();

		const RSAPrivateCrtKey* crt = dynamic_cast<const RSAPrivateCrtKey*>(rsa);

		if (crt)
		{
			_pair.p = crt->getPrimeP();
			_pair.q = crt->getPrimeQ();
			_pair.dp = crt->getPrimeExponentP();
			_pair.dq = crt->getPrimeExponentQ();
			_pair.qi = crt->getCrtCoefficient();
			_crt = true;
		}
		else
			_crt = false;

		/* reset the hash function */
		hashFunctionContextReset(&_hfc);

		_srng = random;
	}
	else
		throw InvalidKeyException("key must be a RSAPrivateKey");
}

void PKCS1RSASignature::engineInitVerify(const PublicKey& key) throw (InvalidKeyException)
{
	const RSAPublicKey* rsa = dynamic_cast<const RSAPublicKey*>(&key);

	if (rsa)
	{
		/* copy key information */
		_pair.n = rsa->getModulus();
		_pair.e = rsa->getPublicExponent();

		/* reset the hash function */
		hashFunctionContextReset(&_hfc);

		_srng = 0;
	}
	else
		throw InvalidKeyException("key must be a RSAPrivateKey");
}

void PKCS1RSASignature::engineUpdate(byte b)
{
	hashFunctionContextUpdate(&_hfc, &b, 1);
}

void PKCS1RSASignature::engineUpdate(const byte* data, size_t offset, size_t len)
{
	hashFunctionContextUpdate(&_hfc, data+offset, len);
}

bytearray* PKCS1RSASignature::engineSign() throw (SignatureException)
{
	size_t sigsize = (_pair.n.bitlength()+7) >> 3;

	bytearray* signature = new bytearray(sigsize);

	engineSign(signature->data(), 0, signature->size());

	return signature;
}

size_t PKCS1RSASignature::engineSign(byte* signature, size_t offset, size_t len) throw (ShortBufferException, SignatureException)
{
	if (!signature)
		throw NullPointerException();

	size_t sigsize = (_pair.n.bitlength()+7) >> 3;

	/* test if we have enough space in output buffer */
	if (sigsize > (len - offset))
		throw ShortBufferException();

	/* okay, we can continue */
	mpnumber c, m;
	bytearray em(sigsize);

	if (pkcs1_emsa_encode_digest(&_hfc, em.data(), sigsize))
		throw SignatureException("internal error in emsa_pkcs1_encode_digest");

	mpnsetbin(&c, em.data(), sigsize);

	if (_crt)
	{
		if (rsapricrt(&_pair.n, &_pair.p, &_pair.q, &_pair.dp, &_pair.dq, &_pair.qi, &c, &m))
			throw SignatureException("internal error in rsapricrt function");
	}
	else
	{
		if (rsapri(&_pair.n, &_pair.d, &c, &m))
			throw SignatureException("internal error in rsapri function");
	}

	if (i2osp(signature+offset, sigsize, m.data, m.size))
		throw SignatureException("internal error in i2osp");

	return sigsize;
}

size_t PKCS1RSASignature::engineSign(bytearray& signature) throw (SignatureException)
{
	size_t sigsize = (_pair.n.bitlength()+7) >> 3;

	signature.resize(sigsize);

	return engineSign(signature.data(), 0, signature.size());
}

bool PKCS1RSASignature::engineVerify(const byte* signature, size_t offset, size_t len) throw (SignatureException)
{
	if (!signature)
		throw NullPointerException();

	size_t sigsize = (_pair.n.bitlength()+7) >> 3;

	/* test if we have enough data in signature */
	if (sigsize > (len - offset))
		return false;

	/* okay, we can continue */
	mpnumber c, m;
	bytearray em(sigsize);

	if (pkcs1_emsa_encode_digest(&_hfc, em.data(), sigsize))
		throw SignatureException("internal error in emsa_pkcs1_encode_digest");

	mpnsetbin(&c, em.data(), sigsize);
	mpnsetbin(&m, signature+offset, sigsize);

	return rsavrfy(&_pair.n, &_pair.e, &m, &c);
}
