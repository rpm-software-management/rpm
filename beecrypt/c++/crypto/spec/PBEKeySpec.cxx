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

#include "beecrypt/c++/crypto/spec/PBEKeySpec.h"

using namespace beecrypt::crypto::spec;

PBEKeySpec::PBEKeySpec(const array<javachar>* password) : _password(password ? *password : 0)
{
	_salt = 0;
	_iteration_count = 0;
	_key_length = 0;
}

PBEKeySpec::PBEKeySpec(const array<javachar>* password, const bytearray* salt, size_t iterationCount, size_t keyLength) : _password(password ? *password : 0)
{
	if (salt)
		_salt = new bytearray(*salt);
	_iteration_count = iterationCount;
	_key_length = keyLength;
}

PBEKeySpec::~PBEKeySpec()
{
}

const array<javachar>& PBEKeySpec::getPassword() const throw ()
{
	return _password;
}

const bytearray* PBEKeySpec::getSalt() const throw ()
{
	return _salt;
}

size_t PBEKeySpec::getIterationCount() const throw ()
{
	return _iteration_count;
}

size_t PBEKeySpec::getKeyLength() const throw ()
{
	return _key_length;
}
