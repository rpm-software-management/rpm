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
#include "beecrypt/c++/provider/DSAKeyPairGenerator.h"
#include "beecrypt/c++/provider/DSAPublicKeyImpl.h"
#include "beecrypt/c++/provider/DSAPrivateKeyImpl.h"
#include "beecrypt/c++/security/KeyPair.h"
using beecrypt::security::KeyPair;
#include "beecrypt/c++/security/spec/DSAParameterSpec.h"
using beecrypt::security::spec::DSAParameterSpec;

namespace {
	const char* P_512 = "fca682ce8e12caba26efccf7110e526db078b05edecbcd1eb4a208f3ae1617ae01f35b91a47e6df63413c5e12ed0899bcd132acd50d99151bdc43ee737592e17";
	const char* Q_512 = "962eddcc369cba8ebb260ee6b6a126d9346e38c5";
	const char* G_512 = "678471b27a9cf44ee91a49c5147db1a9aaf244f05a434d6486931d2d14271b9e35030b71fd73da179069b32e2935630e1c2062354d0da20a6c416e50be794ca4";

	const char* P_768 = "e9e642599d355f37c97ffd3567120b8e25c9cd43e927b3a9670fbec5d890141922d2c3b3ad2480093799869d1e846aab49fab0ad26d2ce6a22219d470bce7d777d4a21fbe9c270b57f607002f3cef8393694cf45ee3688c11a8c56ab127a3daf";
	const char* Q_768 = "9cdbd84c9f1ac2f38d0f80f42ab952e7338bf511";
	const char* G_768 = "30470ad5a005fb14ce2d9dcd87e38bc7d1b1c5facbaecbe95f190aa7a31d23c4dbbcbe06174544401a5b2c020965d8c2bd2171d3668445771f74ba084d2029d83c1c158547f3a9f1a2715be23d51ae4d3e5a1f6a7064f316933a346d3f529252";

	const char* P_1024 = "fd7f53811d75122952df4a9c2eece4e7f611b7523cef4400c31e3f80b6512669455d402251fb593d8d58fabfc5f5ba30f6cb9b556cd7813b801d346ff26660b76b9950a5a49f9fe8047b1022c24fbba9d7feb7c61bf83b57e7c6a8a6150f04fb83f6d3c51ec3023554135a169132f675f3ae2b61d72aeff22203199dd14801c7";
	const char* Q_1024 = "9760508f15230bccb292b982a2eb840bf0581cf5";
	const char* G_1024 = "f7e1a085d69b3ddecbbcab5c36b857b97994afbbfa3aea82f9574c0b3d0782675159578ebad4594fe67107108180b449167123e84c281613b7cf09328cc8a6e13c167a8b547c8d28e0a3ae1e2bb3a675916ea37f0bfa213562f1fb627a01243bcca4f1bea8519089a883dfe15ae59f06928b665e807b552564014c3bfecf492a";
};

using namespace beecrypt::provider;

DSAKeyPairGenerator::DSAKeyPairGenerator()
{
	_size = 0;
	_spec = 0;
	_srng = 0;
}

DSAKeyPairGenerator::~DSAKeyPairGenerator()
{
	_size = 0;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = 0;
}

KeyPair* DSAKeyPairGenerator::genpair(randomGeneratorContext* rngc)
{
	dsaparam param;
	mpnumber x;
	mpnumber y;

	if (_spec)
	{
		param.p = _spec->getP();
		param.q = _spec->getQ();
		param.g = _spec->getG();
	}
	else
	{
		if (_size == 512)
		{
			mpbsethex(&param.p, P_512);
			mpbsethex(&param.q, Q_512);
			mpnsethex(&param.g, G_512);
		}
		else if (_size == 768)
		{
			mpbsethex(&param.p, P_768);
			mpbsethex(&param.q, Q_768);
			mpnsethex(&param.g, G_768);
		}
		else if ((_size == 1024) || !_size)
		{
			mpbsethex(&param.p, P_1024);
			mpbsethex(&param.q, Q_1024);
			mpnsethex(&param.g, G_1024);
		}
		else
		{
			if (dsaparamMake(&param, rngc, _size))
				throw "unexpected error in dsaparamMake";
		}
	}

	if (dldp_pPair(&param, rngc, &x, &y))
		throw "unexpected error in dldp_pPair";

	KeyPair* result = new KeyPair(new DSAPublicKeyImpl(param, y), new DSAPrivateKeyImpl(param, x));

	x.wipe();

	return result;
}

KeyPair* DSAKeyPairGenerator::engineGenerateKeyPair()
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

void DSAKeyPairGenerator::engineInitialize(const AlgorithmParameterSpec& spec, SecureRandom* random) throw (InvalidAlgorithmParameterException)
{
	const DSAParameterSpec* dsaspec = dynamic_cast<const DSAParameterSpec*>(&spec);

	if (dsaspec)
	{
		if (_spec)
			delete _spec;

		_spec = new DSAParameterSpec(*dsaspec);
		_srng = random;
	}
	else
		throw InvalidAlgorithmParameterException("not a DSAParameterSpec");
}

void DSAKeyPairGenerator::engineInitialize(size_t keysize, SecureRandom* random) throw (InvalidParameterException)
{
	if ((keysize < 512) || (keysize > 1024) || ((keysize & 0x3f) != 0))
		throw InvalidParameterException("Prime size must range from 512 to 1024 bits and be a multiple of 64");

	_size = keysize;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = random;
}
