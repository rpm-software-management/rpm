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

#include "beecrypt/c++/io/ByteArrayInputStream.h"
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;

using namespace beecrypt::io;

ByteArrayInputStream::ByteArrayInputStream(const bytearray& b) : _buf(b)
{
	_lock.init();
	_count = _buf.size();
	_mark = 0;
	_pos = 0;
}

ByteArrayInputStream::ByteArrayInputStream(const byte* data, size_t offset, size_t length) : _buf(data+offset, length)
{
	_lock.init();
	_count = _buf.size();
	_mark = 0;
	_pos = 0;
}

ByteArrayInputStream::~ByteArrayInputStream()
{
	_lock.destroy();
}

off_t ByteArrayInputStream::available() throw (IOException)
{
	return (off_t)(_count - _pos);
}

void ByteArrayInputStream::close() throw (IOException)
{
}

void ByteArrayInputStream::mark(off_t readlimit) throw ()
{
	_mark = _pos;
}

bool ByteArrayInputStream::markSupported() throw ()
{
	return true;
}

int ByteArrayInputStream::read() throw (IOException)
{
	register int rc;
	_lock.lock();
	rc = (_pos < _count) ? _buf[_pos++] : -1;
	_lock.unlock();
	return rc;
}

int ByteArrayInputStream::read(byte* data, size_t offset, size_t length) throw (IOException)
{
	if (!data)
		throw NullPointerException();

	_lock.lock();
	if (_pos >= _count)
	{
		_lock.unlock();
		return -1;
	}

	if (_pos + length > _count)
		length = _count - _pos;

	if (length == 0)
	{
		_lock.unlock();
		return 0;
	}

	memcpy(data+offset, _buf.data()+_pos, length);
	_pos += length;

	_lock.unlock();

	return length;
}

int ByteArrayInputStream::read(bytearray& b) throw (IOException)
{
	return read(b.data(), 0, b.size());
}

void ByteArrayInputStream::reset() throw (IOException)
{
	_lock.lock();
	_pos = _mark;
	_lock.unlock();
}

off_t ByteArrayInputStream::skip(off_t n) throw (IOException)
{
	_lock.lock();
	if (_pos + n > _count)
		n = _count - _pos;
	_pos += n;
	_lock.unlock();
	return n;
}
