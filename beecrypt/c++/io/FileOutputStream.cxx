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

#include "beecrypt/c++/io/FileOutputStream.h"
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;

using namespace beecrypt::io;

FileOutputStream::FileOutputStream(FILE *f)
{
	_f = f;
}

FileOutputStream::~FileOutputStream()
{
}

void FileOutputStream::close() throw (IOException)
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

void FileOutputStream::flush() throw (IOException)
{
	if (!_f)
		throw IOException("no valid file handle to flush");

	if (fflush(_f))
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("fflush failed");
		#endif
}

void FileOutputStream::write(byte b) throw (IOException)
{
	if (!_f)
		throw IOException("no valid file handle to write");

	size_t rc = fwrite(&b, 1, 1, _f);

	if (rc < 1)
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("incomplete fwrite");
		#endif
}

void FileOutputStream::write(const byte* data, size_t offset, size_t length) throw (IOException)
{
	if (!data)
		throw NullPointerException();

	if (!_f)
		throw IOException("no valid file handle to write");

	size_t rc = fwrite(data+offset, 1, length, _f);

	if (rc < length)
		#if HAVE_ERRNO_H
		throw IOException(strerror(errno));
		#else
		throw IOException("incomplete fwrite");
		#endif
}

void FileOutputStream::write(const bytearray& b) throw (IOException)
{
	write(b.data(), 0, b.size());
}
