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

#include "beecrypt/c++/security/KeyFactory.h"
#include "beecrypt/c++/security/Security.h"

using namespace beecrypt::security;

KeyFactory::KeyFactory(KeyFactorySpi* spi, const String& algorithm, const Provider& provider)
{
	_kspi = spi;
	_algo = algorithm;
	_prov = &provider;
}

KeyFactory::~KeyFactory()
{
	delete _kspi;
}

KeyFactory* KeyFactory::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
    Security::spi* tmp = Security::getSpi(algorithm, "KeyFactory");

    KeyFactory* result = new KeyFactory((KeyFactorySpi*) tmp->cspi, tmp->name, tmp->prov);

    delete tmp;

    return result;
}

KeyFactory* KeyFactory::getInstance(const String& algorithm, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
    Security::spi* tmp = Security::getSpi(algorithm, "KeyFactory", provider);

    KeyFactory* result = new KeyFactory((KeyFactorySpi*) tmp->cspi, tmp->name, tmp->prov);

    delete tmp;

    return result;
}

KeyFactory* KeyFactory::getInstance(const String& algorithm, const Provider& provider) throw (NoSuchAlgorithmException)
{
    Security::spi* tmp = Security::getSpi(algorithm, "KeyFactory", provider);

    KeyFactory* result = new KeyFactory((KeyFactorySpi*) tmp->cspi, tmp->name, tmp->prov);

    delete tmp;

    return result;
}

PrivateKey* KeyFactory::generatePrivate(const KeySpec& spec) throw (InvalidKeySpecException)
{
	return _kspi->engineGeneratePrivate(spec);
}

PublicKey* KeyFactory::generatePublic(const KeySpec& spec) throw (InvalidKeySpecException)
{
	return _kspi->engineGeneratePublic(spec);
}

KeySpec* KeyFactory::getKeySpec(const Key& key, const type_info& info) throw (InvalidKeySpecException)
{
	return _kspi->engineGetKeySpec(key, info);
}

Key* KeyFactory::translateKey(const Key& key) throw (InvalidKeyException)
{
	return _kspi->engineTranslateKey(key);
}

const String& KeyFactory::getAlgorithm() const throw ()
{
	return _algo;
}

const Provider& KeyFactory::getProvider() const throw ()
{
	return *_prov;
}
