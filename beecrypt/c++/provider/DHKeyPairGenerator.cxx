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
#include "beecrypt/c++/crypto/spec/DHParameterSpec.h"
#include "beecrypt/c++/provider/DHKeyPairGenerator.h"
#include "beecrypt/c++/provider/DHPublicKeyImpl.h"
#include "beecrypt/c++/provider/DHPrivateKeyImpl.h"
#include "beecrypt/c++/provider/BeeCryptProvider.h"
#include "beecrypt/c++/security/KeyPair.h"

#include "beecrypt/dldp.h"

/* precomputed safe primes; it's easy to create generators for these;
 *
 * using a dldp_p struct, set p from the hex value; set q = p/2 and r = 2
 * then call dldp_pgonGenerator.
 */
namespace {
	const char* P_2048 = "fd12e8b7e096a28a00fb548035953cf0eba64ceb5dff0f5672d376d59c196da729f6b5586f18e6f3f1a86c73c5b15662f59439613b309e52aa257488619e5f76a7c4c3f7a426bdeac66bf88343482941413cef06256b39c62744dcb97e7b78e36ec6b885b143f6f3ad0a1cd8a5713e338916613892a264d4a47e72b583fbdaf5bce2bbb0097f7e65cbc86d684882e5bb8196d522dcacd6ad00dfbcd8d21613bdb59c485a65a58325d792272c09ad1173e12c98d865adb4c4d676ada79830c58c37c42dff8536e28f148a23f296513816d3dfed0397a3d4d6e1fa24f07e1b01643a68b4274646a3b876e810206eddacea2b9ef7636a1da5880ef654288b857ea3";
	const char* P_1024 = "e64a3deeddb723e2e4db54c2b09567d196367a86b3b302be07e43ffd7f2e016f866de5135e375bdd2fba6ea9b4299010fafa36dc6b02ba3853cceea07ee94bfe30e0cc82a69c73163be26e0c4012dfa0b2839c97d6cd71eee59a303d6177c6a6740ca63bd04c1ba084d6c369dc2fbfaeebe951d58a4824de52b580442d8cae77";
};

using namespace beecrypt::provider;

DHKeyPairGenerator::DHKeyPairGenerator()
{
	_size = 0;
	_spec = 0;
	_srng = 0;
}

DHKeyPairGenerator::~DHKeyPairGenerator()
{
	_size = 0;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = 0;
}

KeyPair* DHKeyPairGenerator::genpair(randomGeneratorContext* rngc)
{
	dhparam param;
	size_t l;
	mpnumber x;
	mpnumber y;

	if (_spec)
	{
		param.p = _spec->getP();
		param.g = _spec->getG();
		l = _spec->getL();
	}
	else
	{
		if (_size == 2048)
		{
			mpbsethex(&param.p, P_2048);
		}
		else if (_size == 1024 || _size == 0)
		{
			mpbsethex(&param.p, P_1024);
		}

		if (_size == 2048 || _size == 1024 || _size == 0)
		{
			mpnumber q;

			/* set q to half of P */
			mpnset(&q, param.p.size, param.p.modl);
			mpdivtwo(q.size, q.data);
			mpbset(&param.q, q.size, q.data);
			/* set r to 2 */
			mpnsetw(&param.r, 2);

			/* make a generator, order n */
			dldp_pgonGenerator(&param, rngc);
		}
		else
		{
			if (dldp_pgonMakeSafe(&param, rngc, _size))
				throw "unexpected error in dldp_pMakeSafe";
		}
	}

	if (_spec && _spec->getL())
		dldp_pPair_s(&param, rngc, &x, &y, _spec->getL());
	else
		dldp_pPair  (&param, rngc, &x, &y);

	KeyPair* result = new KeyPair(new DHPublicKeyImpl(param, y), new DHPrivateKeyImpl(param, x));

	x.wipe();

	return result;
}

KeyPair* DHKeyPairGenerator::engineGenerateKeyPair()
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

void DHKeyPairGenerator::engineInitialize(const AlgorithmParameterSpec& spec, SecureRandom* random) throw (InvalidAlgorithmParameterException)
{
	const DHParameterSpec* dhspec = dynamic_cast<const DHParameterSpec*>(&spec);

	if (dhspec)
	{
		if (_spec)
			delete _spec;

		_spec = new DHParameterSpec(*dhspec);
		_srng = random;
	}
	else
		throw InvalidAlgorithmParameterException("not a DHParameterSpec");
}

void DHKeyPairGenerator::engineInitialize(size_t keysize, SecureRandom* random) throw (InvalidParameterException)
{
	if (keysize < 768)
		throw InvalidParameterException("Safe prime size must be at least 768 bits");

	_size = keysize;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = random;
}
