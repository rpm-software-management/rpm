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

#include "beecrypt/c++/security/MessageDigest.h"
#include "beecrypt/c++/security/Security.h"

using namespace beecrypt::security;

MessageDigest::MessageDigest(MessageDigestSpi* spi, const String& algorithm, const Provider& provider)
{
	_mspi = spi;
	_algo = algorithm;
	_prov = &provider;
}

MessageDigest::~MessageDigest()
{
	delete _mspi;
}

MessageDigest* MessageDigest::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "MessageDigest");

	MessageDigest* result = new MessageDigest((MessageDigestSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

MessageDigest* MessageDigest::getInstance(const String& algorithm, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "MessageDigest", provider);

	MessageDigest* result = new MessageDigest((MessageDigestSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

MessageDigest* MessageDigest::getInstance(const String& algorithm, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "MessageDigest", provider);

	MessageDigest* result = new MessageDigest((MessageDigestSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

MessageDigest* MessageDigest::clone() const
{
	MessageDigestSpi* _mspc = _mspi->clone();

	if (_mspc)
		return new MessageDigest(_mspc, _algo, *_prov);
	else
		return 0;
}

const bytearray& MessageDigest::digest()
{
	return _mspi->engineDigest();
}

const bytearray& MessageDigest::digest(const bytearray& b)
{
	_mspi->engineUpdate(b.data(), 0, b.size());
	return _mspi->engineDigest();
}

size_t MessageDigest::digest(byte* data, size_t offset, size_t length) throw (ShortBufferException)
{
	return _mspi->engineDigest(data, offset, length);
}

size_t MessageDigest::getDigestLength()
{
	return _mspi->engineGetDigestLength();
}

void MessageDigest::reset()
{
	_mspi->engineReset();
}

void MessageDigest::update(byte b)
{
	_mspi->engineUpdate(b);
}

void MessageDigest::update(const bytearray& b)
{
	_mspi->engineUpdate(b.data(), 0, b.size());
}

void MessageDigest::update(const byte* data, size_t offset, size_t length)
{
	_mspi->engineUpdate(data, offset, length);
}

const String& MessageDigest::getAlgorithm() const throw ()
{
	return _algo;
}

const Provider& MessageDigest::getProvider() const throw ()
{
	return *_prov;
}
