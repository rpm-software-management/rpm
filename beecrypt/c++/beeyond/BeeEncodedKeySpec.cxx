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

#include "beecrypt/c++/beeyond/BeeEncodedKeySpec.h"

using namespace beecrypt::beeyond;

BeeEncodedKeySpec::BeeEncodedKeySpec(const byte* data, size_t size) : EncodedKeySpec(data, size)
{
}

BeeEncodedKeySpec::BeeEncodedKeySpec(const bytearray& copy) : EncodedKeySpec(copy)
{
}

BeeEncodedKeySpec::~BeeEncodedKeySpec()
{
}

const String& BeeEncodedKeySpec::getFormat() const throw ()
{
	static const String* format = 0;

	if (!format)
		format = new String("BEE");

	return *format;
}
