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
#include "beecrypt/c++/provider/DHParameterGenerator.h"
#include "beecrypt/c++/security/AlgorithmParameters.h"
using beecrypt::security::AlgorithmParameters;
#include "beecrypt/c++/crypto/spec/DHParameterSpec.h"
using beecrypt::crypto::spec::DHParameterSpec;

using namespace beecrypt::provider;

DHParameterGenerator::DHParameterGenerator()
{
	_size = 0;
	_spec = 0;
	_srng = 0;
}
 
DHParameterGenerator::~DHParameterGenerator()
{
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_size = 0;
	_srng = 0;
}

AlgorithmParameters* DHParameterGenerator::engineGenerateParameters()
{
	if (!_spec)
	{
		dldp_p param;

		if (_srng)
		{
			randomGeneratorContextAdapter rngc(_srng);
			if (dldp_pgonMakeSafe(&param, &rngc, _size))
				throw "unexpected error in dldp_pMake";
		}
		else
		{
			randomGeneratorContext rngc(randomGeneratorDefault());
			if (dldp_pgonMakeSafe(&param, &rngc, _size))
				throw "unexpected error in dldp_pMake";
		}

		_spec = new DHParameterSpec(param.p, param.g);
	}

	try
	{
		AlgorithmParameters* param = AlgorithmParameters::getInstance("DH");

		param->init(*_spec);

		return param;
	}
	catch (Exception* ex)
	{
		// shouldn't happen
		delete ex;
	}

	return 0;
}

void DHParameterGenerator::engineInit(const AlgorithmParameterSpec& spec, SecureRandom* random) throw (InvalidAlgorithmParameterException)
{
	const DHParameterSpec* dhspec = dynamic_cast<const DHParameterSpec*>(&spec);

	if (dhspec)
	{
		if (_spec)
		{
			delete _spec;
			_spec = 0;
		}

		_spec = new DHParameterSpec(*dhspec);

		_srng = random;
	}
	else
		throw InvalidAlgorithmParameterException("expected DHParameterSpec");
}

void DHParameterGenerator::engineInit(size_t keysize, SecureRandom* random) throw (InvalidParameterException)
{
	if ((keysize < 768) || ((keysize & 0x3f) != 0))
		throw InvalidParameterException("Prime size must be greater than 768 and be a multiple of 64");

	_size = keysize;
	if (_spec)
	{
		delete _spec;
		_spec = 0;
	}
	_srng = random;
}
