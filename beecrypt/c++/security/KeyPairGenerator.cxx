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

#define BEECRYPT_CXX_DLL_EXPORT

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/security/KeyPairGenerator.h"
#include "beecrypt/c++/security/Security.h"

using namespace beecrypt::security;

KeyPairGenerator::KeyPairGenerator(KeyPairGeneratorSpi* spi, const String& algorithm, const Provider& provider)
{
	_kspi = spi;
	_algo = algorithm;
	_prov = &provider;
}

KeyPairGenerator::~KeyPairGenerator()
{
	delete _kspi;
}

KeyPairGenerator* KeyPairGenerator::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "KeyPairGenerator");

	KeyPairGenerator* result = new KeyPairGenerator((KeyPairGeneratorSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

KeyPairGenerator* KeyPairGenerator::getInstance(const String& algorithm, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "KeyPairGenerator", provider);

	KeyPairGenerator* result = new KeyPairGenerator((KeyPairGeneratorSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

KeyPairGenerator* KeyPairGenerator::getInstance(const String& algorithm, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "KeyPairGenerator", provider);

	KeyPairGenerator* result = new KeyPairGenerator((KeyPairGeneratorSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

KeyPair* KeyPairGenerator::generateKeyPair()
{
	return _kspi->engineGenerateKeyPair();
}

void KeyPairGenerator::initialize(const AlgorithmParameterSpec& spec) throw (InvalidAlgorithmParameterException)
{
	_kspi->engineInitialize(spec, 0);
}

void KeyPairGenerator::initialize(const AlgorithmParameterSpec& spec, SecureRandom* random) throw (InvalidAlgorithmParameterException)
{
	_kspi->engineInitialize(spec, random);
}

void KeyPairGenerator::initialize(size_t keysize) throw (InvalidParameterException)
{
	_kspi->engineInitialize(keysize, 0);
}

void KeyPairGenerator::initialize(size_t keysize, SecureRandom* random) throw (InvalidParameterException)
{
	_kspi->engineInitialize(keysize, random);
}

const String& KeyPairGenerator::getAlgorithm() const throw ()
{
	return _algo;
}

const Provider& KeyPairGenerator::getProvider() const throw ()
{
	return *_prov;
}
