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

#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/provider/MD5Digest.h"

using namespace beecrypt::provider;

MD5Digest::MD5Digest() : _digest(16)
{
	md5Reset(&_param);
}

MD5Digest::~MD5Digest()
{
}

MD5Digest* MD5Digest::clone() const
{
	MD5Digest* result = new MD5Digest();

	memcpy(&result->_param, &_param, sizeof(md5Param));

	return result;
}

const bytearray& MD5Digest::engineDigest()
{
	md5Digest(&_param, _digest.data());

	return _digest;
}

size_t MD5Digest::engineDigest(byte* data, size_t offset, size_t length) throw (ShortBufferException)
{
	if (!data)
		throw NullPointerException();

	if (length < 16)
		throw ShortBufferException();

	md5Digest(&_param, data);

	return 16;
}

size_t MD5Digest::engineGetDigestLength()
{
	return 16;
}

void MD5Digest::engineReset()
{
	md5Reset(&_param);
}

void MD5Digest::engineUpdate(byte b)
{
	md5Update(&_param, &b, 1);
}

void MD5Digest::engineUpdate(const byte* data, size_t offset, size_t length)
{
	md5Update(&_param, data+offset, length);
}
