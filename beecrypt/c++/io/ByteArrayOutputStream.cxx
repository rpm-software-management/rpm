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

#include "beecrypt/c++/io/ByteArrayOutputStream.h"
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;

using namespace beecrypt::io;

ByteArrayOutputStream::ByteArrayOutputStream() : _buf(32)
{
	_lock.init();
	_count = 0;
}

ByteArrayOutputStream::ByteArrayOutputStream(size_t size) : _buf(size)
{
	_lock.init();
	_count = 0;
}

ByteArrayOutputStream::~ByteArrayOutputStream()
{
	_lock.destroy();
}

void ByteArrayOutputStream::reset() throw ()
{
	_count = 0;
}

size_t ByteArrayOutputStream::size() throw ()
{
	return _count;
}

bytearray* ByteArrayOutputStream::toByteArray()
{
	bytearray* result = new bytearray();

	toByteArray(*result);

	return result;
}

void ByteArrayOutputStream::toByteArray(bytearray& b)
{
	_lock.lock();

	b.resize(_count);
	memcpy(b.data(), _buf.data(), _count);

	_lock.unlock();
}

void ByteArrayOutputStream::close() throw (IOException)
{
}

void ByteArrayOutputStream::flush() throw (IOException)
{
}

void ByteArrayOutputStream::write(byte b) throw (IOException)
{
	_lock.lock();

	size_t newcount = _count+1;
	size_t actualsz = _buf.size();

	if (actualsz < newcount)
	{
		if (actualsz == 0)
			_buf.resize(32);
		else
			_buf.resize(actualsz << 1);
	}

	_buf[_count++] = b;

	_lock.unlock();
}

void ByteArrayOutputStream::write(const byte* data, size_t offset, size_t length) throw (IOException)
{
	if (!data)
		throw NullPointerException();

	_lock.lock();

	size_t newcount = _count + length;
	size_t actualsz = _buf.size();

	if (newcount > actualsz)
	{
		_buf.resize(newcount > (actualsz << 1) ? newcount : (actualsz << 1));
	}
	memcpy(_buf.data()+_count, data, length);
	_count += length;
	_lock.unlock();
}

void ByteArrayOutputStream::write(const bytearray& b) throw (IOException)
{
	write(b.data(), 0, b.size());
}
