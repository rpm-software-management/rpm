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

#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/security/DigestInputStream.h"

using namespace beecrypt::security;

DigestInputStream::DigestInputStream(InputStream& in, MessageDigest& m) : FilterInputStream(in), digest(m)
{
	_on = true;
}

DigestInputStream::~DigestInputStream()
{
}

int DigestInputStream::read() throw (IOException)
{
	int rc = in.read();
	if (rc >= 0 && _on)
		digest.update((byte) rc);
	return rc;
}

int DigestInputStream::read(byte *data, size_t offset, size_t length) throw (IOException)
{
	if (!data)
		throw NullPointerException();

	int rc = in.read(data, offset, length);
	if (rc >= 0 && _on)
		digest.update(data, offset, rc);
	return rc;
}

void DigestInputStream::on(bool on)
{
	_on = on;
}

MessageDigest& DigestInputStream::getMessageDigest()
{
	return digest;
}

void DigestInputStream::setMessageDigest(MessageDigest& m)
{
	digest = m;
}
