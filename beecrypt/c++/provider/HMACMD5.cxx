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

#include "beecrypt/pkcs12.h"

#include "beecrypt/c++/crypto/interfaces/PBEKey.h"
using beecrypt::crypto::interfaces::PBEKey;
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/provider/HMACMD5.h"

using namespace beecrypt::provider;

HMACMD5::HMACMD5() : _digest(16)
{
}

HMACMD5::~HMACMD5()
{
}

HMACMD5* HMACMD5::clone() const
{
	HMACMD5* result = new HMACMD5();

	memcpy(&result->_param, &_param, sizeof(hmacmd5Param));

	return result;
}

const bytearray& HMACMD5::engineDoFinal()
{
	hmacmd5Digest(&_param, _digest.data());

	return _digest;
}

size_t HMACMD5::engineDoFinal(byte* data, size_t offset, size_t length) throw (ShortBufferException)
{
	if (!data)
		throw NullPointerException();

	if (length < 16)
		throw ShortBufferException();

	hmacmd5Digest(&_param, data);

	return 16;
}

size_t HMACMD5::engineGetMacLength()
{
	return 16;
}

void HMACMD5::engineReset()
{
	hmacmd5Reset(&_param);
}

void HMACMD5::engineUpdate(byte b)
{
	hmacmd5Update(&_param, &b, 1);
}

void HMACMD5::engineUpdate(const byte* data, size_t offset, size_t length)
{
	hmacmd5Update(&_param, data+offset, length);
}

void HMACMD5::engineInit(const Key& key, const AlgorithmParameterSpec* spec) throw (InvalidKeyException, InvalidAlgorithmParameterException)
{
	if (spec)
		throw InvalidAlgorithmParameterException("No AlgorithmParameterSpec supported");

	const PBEKey* pbe = dynamic_cast<const PBEKey*>(&key);
	if (pbe)
	{
		byte _mac_key[16];
		bytearray _rawk, _salt;
		size_t _iter;

		if (pbe->getEncoded())
			_rawk = *(pbe->getEncoded());
		else
			throw InvalidKeyException("PBEKey must have an encoding");

		if (pbe->getSalt())
			_salt = *(pbe->getSalt());

		_iter = pbe->getIterationCount();

		if (pkcs12_derive_key(&md5, PKCS12_ID_MAC, _rawk.data(), _rawk.size(), _salt.data(), _salt.size(), _iter, _mac_key, 16))
			throw InvalidKeyException("pkcs12_derive_key returned error");

		hmacmd5Setup(&_param, _mac_key, 128);

		return;
	}

	throw InvalidKeyException("Expected a PBEKey");
}
