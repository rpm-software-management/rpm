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

#include "beecrypt/c++/security/SecureRandom.h"
#include "beecrypt/c++/security/SecureRandomSpi.h"
#include "beecrypt/c++/security/Security.h"

using namespace beecrypt::security;

SecureRandom* SecureRandom::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "SecureRandom");

	SecureRandom* result = new SecureRandom((SecureRandomSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

SecureRandom* SecureRandom::getInstance(const String& type, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(type, "SecureRandom", provider);

	SecureRandom* result = new SecureRandom((SecureRandomSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

SecureRandom* SecureRandom::getInstance(const String& type, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(type, "SecureRandom", provider);

	SecureRandom* result = new SecureRandom((SecureRandomSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

void SecureRandom::getSeed(byte* data, size_t size)
{
	entropyGatherNext(data, size);
}

SecureRandom::SecureRandom()
{
	Security::spi* tmp = Security::getFirstSpi("SecureRandom");

	_rspi = (SecureRandomSpi*) tmp->cspi;
	_type = tmp->name;
	_prov = &tmp->prov;

	delete tmp;
}

SecureRandom::SecureRandom(SecureRandomSpi* rspi, const String& type, const Provider& provider) : _prov(&provider)
{
	_rspi = rspi;
	_type = type;
	_prov = &provider;
}

SecureRandom::~SecureRandom()
{
	delete _rspi;
}

void SecureRandom::generateSeed(byte* data, size_t size)
{
	_rspi->engineGenerateSeed(data, size);
}

void SecureRandom::setSeed(const byte* data, size_t size)
{
	_rspi->engineSetSeed(data, size);
}

void SecureRandom::nextBytes(byte* data, size_t size)
{
	_rspi->engineNextBytes(data, size);
}

const String& SecureRandom::getType() const throw ()
{
	return _type;
}

const Provider& SecureRandom::getProvider() const throw ()
{
	return *_prov;
}
