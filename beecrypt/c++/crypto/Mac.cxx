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

#include "beecrypt/c++/crypto/Mac.h"
#include "beecrypt/c++/lang/IllegalArgumentException.h"
using beecrypt::lang::IllegalArgumentException;
#include "beecrypt/c++/security/Security.h"
using beecrypt::security::Security;

using namespace beecrypt::crypto;

Mac::Mac(MacSpi* spi, const String& algorithm, const Provider& provider)
{
	_mspi = spi;
	_algo = algorithm;
	_prov = &provider;
	_init = false;
}

Mac::~Mac()
{
	delete _mspi;
}

Mac* Mac::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "Mac");

	Mac* result = new Mac((MacSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

Mac* Mac::getInstance(const String& algorithm, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "Mac", provider);

	Mac* result = new Mac((MacSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

Mac* Mac::getInstance(const String& algorithm, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "Mac", provider);

	Mac* result = new Mac((MacSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

Mac* Mac::clone() const
{
	MacSpi* _mspc = _mspi->clone();

	if (_mspc)
	{
		// don't forget to also clone the _init state!
		Mac* result = new Mac(_mspc, _algo, *_prov);
		result->_init = _init;
		return result;
	}
	else
		return 0;
}

const bytearray& Mac::doFinal() throw (IllegalStateException)
{
	if (!_init)
		throw IllegalStateException();

	return _mspi->engineDoFinal();
}

const bytearray& Mac::doFinal(const bytearray& b) throw (IllegalStateException)
{
	if (!_init)
		throw IllegalStateException();

	_mspi->engineUpdate(b.data(), 0, b.size());
	return _mspi->engineDoFinal();
}

size_t Mac::doFinal(byte* data, size_t offset, size_t length) throw (IllegalStateException, ShortBufferException)
{
	if (!_init)
		throw IllegalStateException();

	return _mspi->engineDoFinal(data, offset, length);
}

size_t Mac::getMacLength()
{
	return _mspi->engineGetMacLength();
}

void Mac::init(const Key& key) throw (InvalidKeyException)
{
	try
	{
		_mspi->engineInit(key, 0);
	}
	catch (InvalidAlgorithmParameterException)
	{
		throw IllegalArgumentException("Mac apparently requires an AlgorithmParameterSpec");
	}
	_init = true;
}

void Mac::init(const Key& key, const AlgorithmParameterSpec* spec) throw (InvalidKeyException, InvalidAlgorithmParameterException)
{
	_mspi->engineInit(key, spec);
	_init = true;
}

void Mac::reset()
{
	_mspi->engineReset();
}

void Mac::update(byte b) throw (IllegalStateException)
{
	if (!_init)
		throw IllegalStateException();

	_mspi->engineUpdate(b);
}

void Mac::update(const bytearray& b) throw (IllegalStateException)
{
	if (!_init)
		throw IllegalStateException();

	_mspi->engineUpdate(b.data(), 0, b.size());
}

void Mac::update(const byte* data, size_t offset, size_t length) throw (IllegalStateException)
{
	if (!_init)
		throw IllegalStateException();

	_mspi->engineUpdate(data, offset, length);
}

const String& Mac::getAlgorithm() const throw ()
{
	return _algo;
}

const Provider& Mac::getProvider() const throw ()
{
	return *_prov;
}
