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
#include "beecrypt/c++/provider/HMACSHA1.h"

using namespace beecrypt::provider;

HMACSHA1::HMACSHA1() : _digest(20)
{
}

HMACSHA1::~HMACSHA1()
{
}

HMACSHA1* HMACSHA1::clone() const
{
	HMACSHA1* result = new HMACSHA1();

	memcpy(&result->_param, &_param, sizeof(hmacsha1Param));

	return result;
}

const bytearray& HMACSHA1::engineDoFinal()
{
	hmacsha1Digest(&_param, _digest.data());

	return _digest;
}

size_t HMACSHA1::engineDoFinal(byte* data, size_t offset, size_t length) throw (ShortBufferException)
{
	if (!data)
		throw NullPointerException();

	if (length < 20)
		throw ShortBufferException();

	hmacsha1Digest(&_param, data);

	return 20;
}

size_t HMACSHA1::engineGetMacLength()
{
	return 20;
}

void HMACSHA1::engineReset()
{
	hmacsha1Reset(&_param);
}

void HMACSHA1::engineUpdate(byte b)
{
	hmacsha1Update(&_param, &b, 1);
}

void HMACSHA1::engineUpdate(const byte* data, size_t offset, size_t length)
{
	hmacsha1Update(&_param, data+offset, length);
}

void HMACSHA1::engineInit(const Key& key, const AlgorithmParameterSpec* spec) throw (InvalidKeyException, InvalidAlgorithmParameterException)
{
	if (spec)
		throw InvalidAlgorithmParameterException("No AlgorithmParameterSpec supported");

	const PBEKey* pbe = dynamic_cast<const PBEKey*>(&key);
	if (pbe)
	{
		byte _mac_key[20];
		bytearray _rawk, _salt;
		size_t _iter;

		if (pbe->getEncoded())
			_rawk = *(pbe->getEncoded());
		else
			throw InvalidKeyException("PBEKey must have an encoding");

		if (pbe->getSalt())
			_salt = *(pbe->getSalt());

		_iter = pbe->getIterationCount();

		if (pkcs12_derive_key(&sha1, PKCS12_ID_MAC, _rawk.data(), _rawk.size(), _salt.data(), _salt.size(), _iter, _mac_key, 20))
			throw InvalidKeyException("pkcs12_derive_key returned error");

		hmacsha1Setup(&_param, _mac_key, 160);

		return;
	}

	throw InvalidKeyException("Expected a PBEKey");
}
