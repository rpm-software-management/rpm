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

#include "beecrypt/c++/io/DataOutputStream.h"

using namespace beecrypt::io;

DataOutputStream::DataOutputStream(OutputStream& out) : FilterOutputStream(out)
{
	_lock.init();
	_utf = 0;
	written = 0;
}

DataOutputStream::~DataOutputStream()
{
	_lock.destroy();
	if (_utf)
		ucnv_close(_utf);
}

size_t DataOutputStream::size() const throw ()
{
	return written;
}

void DataOutputStream::write(byte b) throw (IOException)
{
	_lock.lock();
	out.write(b);
	written++;
	_lock.unlock();
}

void DataOutputStream::write(const byte* data, size_t offset, size_t len) throw (IOException)
{
	_lock.lock();
	out.write(data, offset, len);
	written += len;
	_lock.unlock();
}

void DataOutputStream::write(const bytearray& b) throw (IOException)
{
	write(b.data(), 0, b.size());
}

void DataOutputStream::writeBoolean(bool b) throw (IOException)
{
	_lock.lock();
	out.write(b ? 1 : 0);
	written++;
	_lock.unlock();
}

void DataOutputStream::writeByte(byte b) throw (IOException)
{
	_lock.lock();
	out.write(b);
	written++;
	_lock.unlock();
}

void DataOutputStream::writeShort(javashort s) throw (IOException)
{
	_lock.lock();
	out.write((s >>  8)       );
	out.write((s      ) & 0xff);
	written += 2;
	_lock.unlock();
}

void DataOutputStream::writeInt(javaint i) throw (IOException)
{
	_lock.lock();
	out.write((i >> 24)       );
	out.write((i >> 16) & 0xff);
	out.write((i >>  8) & 0xff);
	out.write((i      ) & 0xff);
	written += 4;
	_lock.unlock();
}

void DataOutputStream::writeLong(javalong l) throw (IOException)
{
	_lock.lock();
	out.write((l >> 56)       );
	out.write((l >> 48) & 0xff);
	out.write((l >> 40) & 0xff);
	out.write((l >> 32) & 0xff);
	out.write((l >> 24) & 0xff);
	out.write((l >> 16) & 0xff);
	out.write((l >>  8) & 0xff);
	out.write((l      ) & 0xff);
	written += 8;
	_lock.unlock();
}

void DataOutputStream::writeChars(const String& str) throw (IOException)
{
	const UChar* buffer = str.getBuffer();
	size_t len = str.length();

	_lock.lock();
	for (size_t i = 0; i < len; i++)
	{
		out.write((buffer[i] >> 8) & 0xff);
		out.write((buffer[i]     ) & 0xff);
	}
	written += (len << 1);
	_lock.unlock();
}

void DataOutputStream::writeUTF(const String& str) throw (IOException)
{
	UErrorCode status = U_ZERO_ERROR;

	if (!_utf)
	{
		// UTF-8 converter lazy initialization
		_utf = ucnv_open("UTF-8", &status);
		if (U_FAILURE(status))
			throw IOException("unable to open ICU UTF-8 converter");
	}

	// the expected status code here is U_BUFFER_OVERFLOW_ERROR
	size_t need = ucnv_fromUChars(_utf, 0, 0, str.getBuffer(), str.length(), &status);
	if (U_FAILURE(status))
		if (status != U_BUFFER_OVERFLOW_ERROR)
			throw IOException("unexpected error in ucnv_fromUChars");

	if (need > 0xffff)
		throw IOException("String length >= 64K");

	byte* buffer = new byte[need];

	status = U_ZERO_ERROR;

	// the expected status code here is U_STRING_NOT_TERMINATED_WARNING
	ucnv_fromUChars(_utf, (char*) buffer, need, str.getBuffer(), str.length(), &status);
	if (status != U_STRING_NOT_TERMINATED_WARNING)
	{
		delete[] buffer;
		throw IOException("error in ucnv_fromUChars");
	}

	// everything ready for the critical section
	_lock.lock();
	try
	{
		out.write((need >>  8) & 0xff);
		out.write((need      ) & 0xff);
		out.write(buffer, 0, need);
		written += 2 + need;
		_lock.unlock();

		delete[] buffer;
	}
	catch (IOException)
	{
		_lock.unlock();
		delete[] buffer;
		throw;
	}
}
