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

#include "beecrypt/c++/beeyond/BeeOutputStream.h"

using namespace beecrypt::beeyond;

BeeOutputStream::BeeOutputStream(OutputStream& out) : DataOutputStream(out)
{
}

BeeOutputStream::~BeeOutputStream()
{
}

void BeeOutputStream::write(const mpnumber& n) throw (IOException)
{
	size_t bits = n.bitlength();
	size_t length = ((bits + 7) >> 3) + (((bits & 7) == 0) ? 1 : 0);

	byte* buffer = new byte[length];

	try
	{
		i2osp(buffer, length, n.data, n.size);

		DataOutputStream::writeInt(length);
		DataOutputStream::write(buffer, 0, length);

		delete[] buffer;
	}
	catch (IOException)
	{
		delete[] buffer;
		throw;
	}
}

void BeeOutputStream::write(const mpbarrett& b) throw (IOException)
{
	size_t bits = b.bitlength();
	size_t length = ((bits + 7) >> 3) + (((bits & 7) == 0) ? 1 : 0);

	byte* buffer = new byte[length];

	try
	{
		i2osp(buffer, length, b.modl, b.size);

		DataOutputStream::writeInt(length);
		DataOutputStream::write(buffer, 0, length);

		delete[] buffer;
	}
	catch (IOException)
	{
		delete[] buffer;
		throw;
	}
}
