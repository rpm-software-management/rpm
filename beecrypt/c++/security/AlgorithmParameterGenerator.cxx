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

#include "beecrypt/c++/security/AlgorithmParameterGenerator.h"
#include "beecrypt/c++/security/AlgorithmParameterGeneratorSpi.h"
#include "beecrypt/c++/security/AlgorithmParameters.h"
#include "beecrypt/c++/security/Provider.h"
#include "beecrypt/c++/security/Security.h"
#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"

using namespace beecrypt::security;

AlgorithmParameterGenerator::AlgorithmParameterGenerator(AlgorithmParameterGeneratorSpi* spi, const String& algorithm, const Provider& provider)
{
	_aspi = spi;
	_algo = algorithm;
	_prov = &provider;
}

AlgorithmParameterGenerator::~AlgorithmParameterGenerator()
{
	delete _aspi;
}

AlgorithmParameterGenerator* AlgorithmParameterGenerator::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "AlgorithmParameterGenerator");

    AlgorithmParameterGenerator* result = new AlgorithmParameterGenerator((AlgorithmParameterGeneratorSpi*) tmp->cspi, tmp->name, tmp->prov);

    delete tmp;
                                                                                
    return result;
}

AlgorithmParameterGenerator* AlgorithmParameterGenerator::getInstance(const String& algorithm, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "AlgorithmParameterGenerator", provider);

    AlgorithmParameterGenerator* result = new AlgorithmParameterGenerator((AlgorithmParameterGeneratorSpi*) tmp->cspi, tmp->name, tmp->prov);

    delete tmp;
                                                                                
    return result;
}

AlgorithmParameterGenerator* AlgorithmParameterGenerator::getInstance(const String& algorithm, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "AlgorithmParameterGenerator", provider);

    AlgorithmParameterGenerator* result = new AlgorithmParameterGenerator((AlgorithmParameterGeneratorSpi*) tmp->cspi, tmp->name, tmp->prov);

    delete tmp;
                                                                                
    return result;
}

AlgorithmParameters* AlgorithmParameterGenerator::generateParameters() throw (InvalidAlgorithmParameterException)
{
	return _aspi->engineGenerateParameters();
}

void AlgorithmParameterGenerator::init(const AlgorithmParameterSpec& genParamSpec) throw (InvalidAlgorithmParameterException)
{
	_aspi->engineInit(genParamSpec, 0);
}

void AlgorithmParameterGenerator::init(const AlgorithmParameterSpec& genParamSpec, SecureRandom* random) throw (InvalidAlgorithmParameterException)
{
	_aspi->engineInit(genParamSpec, random);
}

void AlgorithmParameterGenerator::init(size_t size) throw (InvalidParameterException)
{
	_aspi->engineInit(size, 0);
}

void AlgorithmParameterGenerator::init(size_t size, SecureRandom* random) throw (InvalidParameterException)
{
	_aspi->engineInit(size, random);
}

const String& AlgorithmParameterGenerator::getAlgorithm() const throw ()
{
	return _algo;
}

const Provider& AlgorithmParameterGenerator::getProvider() const throw ()
{
	return *_prov;
}
