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

#if HAVE_ERRNO_H
# include <errno.h>
#endif

#include "beecrypt/c++/io/FileInputStream.h"
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;

using namespace beecrypt::io;

FileInputStream::FileInputStream(FILE* f)
{
	_f = f;
	_mark = -1;
}

FileInputStream::~FileInputStream()
{
}

off_t FileInputStream::available() throw (IOException)
{
	if (!_f)
		throw IOException("not a valid file handle");

	long _curr, _size;

	if ((_curr = ftell(_f)) == -1)
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("ftell failed");
		#endif

	if (fseek(_f, 0, SEEK_END))
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("fseek failed");
		#endif

	if ((_size = ftell(_f)) == -1)
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("ftell failed");
		#endif

	if (fseek(_f, _curr, SEEK_SET))
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("fseek failed");
		#endif

	return (off_t) (_size - _curr);
}

void FileInputStream::close() throw (IOException)
{
	if (_f)
	{
		if (fclose(_f))
			#if HAVE_ERRNO_H
			throw IOException(strerror(errno));
			#else
			throw IOException("fclose failed");
			#endif

		_f = 0;
	}
}

void FileInputStream::mark(off_t readlimit) throw ()
{
	if (_f)
		_mark = ftell(_f);
}

bool FileInputStream::markSupported() throw ()
{
	return true;
}

int FileInputStream::read() throw (IOException)
{
	if (!_f)
		throw IOException("not a valid file handle");

	return fgetc(_f);
}

int FileInputStream::read(byte* data, size_t offset, size_t length) throw (IOException)
{
	if (!_f)
		throw IOException("not a valid file handle");

	if (!data)
		throw NullPointerException();

	size_t rc = fread(data+offset, 1, length, _f);

	if (rc == 0)
		return -1;

	return rc;
}

int FileInputStream::read(bytearray& b) throw (IOException)
{
	return read(b.data(), 0, b.size());
}

void FileInputStream::reset() throw (IOException)
{
	if (!_f)
		throw IOException("not a valid file handle");

	if (_mark < 0)
		throw IOException("not a valid mark");

	if (fseek(_f, _mark, SEEK_SET))
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("fseek failed");
		#endif
}

off_t FileInputStream::skip(off_t n) throw (IOException)
{
	if (!_f)
		throw IOException("not a valid file handle");

	off_t _avail = available();

	if (n > _avail)
		n = _avail;

	if (fseek(_f, (long) n, SEEK_CUR))
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("fseek failed");
		#endif

	return n;
}
