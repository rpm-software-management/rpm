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

#include "beecrypt/c++/adapter.h"
using beecrypt::randomGeneratorContextAdapter;
#include "beecrypt/c++/provider/BeeCryptProvider.h"
#include "beecrypt/c++/provider/RSAKeyPairGenerator.h"
#include "beecrypt/c++/provider/RSAPublicKeyImpl.h"
#include "beecrypt/c++/provider/RSAPrivateCrtKeyImpl.h"

#include "beecrypt/rsakp.h"

using namespace beecrypt::provider;

RSAKeyPairGenerator::RSAKeyPairGenerator()
{
	_size = 0;
	_spec = 0;
	_srng = 0;
}

RSAKeyPairGenerator::~RSAKeyPairGenerator()
{
	_size = 0;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = 0;
}

KeyPair* RSAKeyPairGenerator::genpair(randomGeneratorContext* rngc)
{
	rsakp _pair;

	if (rsakpMake(&_pair, rngc, _spec ? _spec->getKeysize() : (_size ? _size : 1024)))
		throw "unexpected error in rsakpMake";

	return new KeyPair(new RSAPublicKeyImpl(_pair.n, _pair.e), new RSAPrivateCrtKeyImpl(_pair.n, _pair.e, _pair.d, _pair.p, _pair.q, _pair.dp, _pair.dq, _pair.qi));
}

KeyPair* RSAKeyPairGenerator::engineGenerateKeyPair()
{
	if (_srng)
	{
		randomGeneratorContextAdapter rngc(_srng);

		return genpair(&rngc);
	}
	else
	{
		randomGeneratorContext rngc(randomGeneratorDefault());

		return genpair(&rngc);
	}
}

void RSAKeyPairGenerator::engineInitialize(const AlgorithmParameterSpec& spec, SecureRandom* random) throw (InvalidAlgorithmParameterException)
{
	const RSAKeyGenParameterSpec* rsaspec = dynamic_cast<const RSAKeyGenParameterSpec*>(&spec);

	if (rsaspec)
	{
		if (_spec)
			delete _spec;

		_spec = new RSAKeyGenParameterSpec(rsaspec->getKeysize(), rsaspec->getPublicExponent());
	}
	else
		throw InvalidAlgorithmParameterException("not an RSAKeyGenParameterSpec");
}

void RSAKeyPairGenerator::engineInitialize(size_t keysize, SecureRandom* random) throw (InvalidParameterException)
{
	if (keysize < 512)
		throw InvalidParameterException("Modulus size must be at least 512 bits");

	_size = keysize;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = random;
}
