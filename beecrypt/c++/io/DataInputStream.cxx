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

#include "beecrypt/c++/io/DataInputStream.h"
#include "beecrypt/c++/io/EOFException.h"
#include "beecrypt/c++/io/PushbackInputStream.h"
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;

#define MAX_BYTES_PER_CHARACTER	8

using namespace beecrypt::io;

DataInputStream::DataInputStream(InputStream& in) : FilterInputStream(in)
{
	_pin = &in;
	_del = false;
	_utf = 0;
	_loc = 0;
}

DataInputStream::~DataInputStream()
{
	if (_utf)
	{
		ucnv_close(_utf);
		_utf = 0;
	}

	if (_loc)
	{
		ucnv_close(_loc);
		_loc = 0;
	}

	if (_del)
	{
		delete _pin;
		_pin = 0;
	}
}

bool DataInputStream::readBoolean() throw (IOException)
{
	register int b = _pin->read();

	if (b < 0)
		throw EOFException();

	return (b != 0);
}

javabyte DataInputStream::readByte() throw (IOException)
{
	register int b = _pin->read();

	if (b < 0)
		throw EOFException();

	return static_cast<javabyte>(b);
}

int DataInputStream::readUnsignedByte() throw (IOException)
{
	register int b = _pin->read();

	if (b < 0)
		throw EOFException();

	return b;
}

javashort DataInputStream::readShort() throw (IOException)
{
	register javashort tmp = 0;
	register int rc;

	for (register unsigned i = 0; i < 2; i++)
	{
		if ((rc = _pin->read()) < 0)
			throw EOFException();

		tmp = (tmp << 8) + rc;
	}

	return tmp;
}

int DataInputStream::readUnsignedShort() throw (IOException)
{
	register int tmp = 0, rc;

	for (register unsigned i = 0; i < 2; i++)
	{
		if ((rc = _pin->read()) < 0)
			throw EOFException();

		tmp = (tmp << 8) + rc;
	}

	return tmp;
}

javachar DataInputStream::readChar() throw (IOException)
{
	register javachar tmp = 0;
	register int rc;

	for (register unsigned i = 0; i < 2; i++)
	{
		if ((rc = _pin->read()) < 0)
			throw EOFException();

		tmp = (tmp << 8) + rc;
	}

	return tmp;
}

javaint DataInputStream::readInt() throw (IOException)
{
	register javaint tmp = 0;
	register int rc;

	for (register unsigned i = 0; i < 4; i++)
	{
		if ((rc = _pin->read()) < 0)
			throw EOFException();

		tmp = (tmp << 8) + rc;
	}

	return tmp;
}

javalong DataInputStream::readLong() throw (IOException)
{
	register javalong tmp = 0;
	register int rc;

	for (register unsigned i = 0; i < 8; i++)
	{
		if ((rc = _pin->read()) < 0)
			throw EOFException();

		tmp = (tmp << 8) + rc;
	}

	return tmp;
}

void DataInputStream::readUTF(String& str) throw (IOException)
{
	UErrorCode status = U_ZERO_ERROR;

	if (!_utf)
	{
		// UTF-8 converter lazy initialization
		_utf = ucnv_open("UTF-8", &status);
		if (U_FAILURE(status))
			throw IOException("unable to open ICU UTF-8 converter");
	}

	int utflen = readUnsignedShort();

	if (utflen > 0)
	{
		byte* data = new byte[utflen];

		readFully(data, 0, utflen);

		status = U_ZERO_ERROR;
		size_t ulen = ucnv_toUChars(_utf, 0, 0, (const char*) data, (size_t) utflen, &status);
		if (status != U_BUFFER_OVERFLOW_ERROR)
		{
			delete[] data;
			throw "error in ucnv_toUChars";
		}

		UChar* buffer = str.getBuffer(ulen+1);

		if (buffer)
		{
			status = U_ZERO_ERROR;
			ucnv_toUChars(_utf, buffer, ulen+1, (const char*) data, (size_t) utflen, &status);

			delete[] data;

			if (status != U_ZERO_ERROR)
				throw "error in ucnv_toUChars";

			str.releaseBuffer(ulen);
		}
		else
		{
			delete[] data;
			throw "error in String::getBuffer(size_t)";
		}
	}
}

String* DataInputStream::readUTF() throw (IOException)
{
	String* str = new String();

	try
	{
		readUTF(*str);
	}
	catch (IOException ex)
	{
		/* cleanup str */
		delete str;
		/* re-throw exception */
		throw ex;
	}
	return str;
}

String* DataInputStream::readLine() throw (IOException)
{
	String* result = new String();

	readLine(*result);

	return result;
}

void DataInputStream::readLine(String& line) throw (IOException)
{
	UErrorCode status = U_ZERO_ERROR;

	if (!_loc)
	{
		// default locale converter lazy initialization
		_loc = ucnv_open(0, &status);
		if (U_FAILURE(status))
			throw IOException("unable to open ICU default locale converter");
	}

	      UChar  target_buffer[1];
	      UChar* target = target_buffer;
	const UChar* target_limit = target_buffer+1;
	      char  source_buffer[MAX_BYTES_PER_CHARACTER];
	const char* source = source_buffer;
	      char* source_limit = source_buffer;

	bool cr = false;

	int ch;

	// clear the line
	line.remove();

	do
	{
		ch = _pin->read();

		if (ch >= 0)
		{
			if ((source_limit-source_buffer) == MAX_BYTES_PER_CHARACTER)
				throw IOException("fubar in readLine");

			*(source_limit++) = (byte) ch;
		}

		status = U_ZERO_ERROR;
		// use the default locale converter; flush if ch == -1
		ucnv_toUnicode(_loc, &target, target_limit, &source, source_limit, NULL, (UBool) (ch == -1), &status);

		if (U_FAILURE(status))
			throw IOException("error in ucnv_toUnicode");

		if (target == target_limit)
		{
			// we got a whole character from the converter
			if (cr)
			{
				// last character read was ASCII <CR>; is this one a <LF>?
				if (target_buffer[0] != 0x0A)
				{
					// unread the right number of bytes 
					PushbackInputStream* p = dynamic_cast<PushbackInputStream*>(_pin);
					if (p)
						p->unread((const byte*) source_buffer, 0, source-source_buffer);
					else
						throw IOException("fubar in dynamic_cast");
				}
				// we're now officially at the end of the line
				break;
			}

			// did we get an ASCII <LF>?
			if (target_buffer[0] == 0x0A)
				break;

			// did we get an ASCII <CR>?
			if (target_buffer[0] == 0x0D)
			{
				cr = true;

				// the next character may be a <LF> but if not we'll have to 'unread' it
				if (!_del)
				{
					// lazy push
					_pin = new PushbackInputStream(in, MAX_BYTES_PER_CHARACTER);
					_del = true;
				}
			}
			else
			{
				// append character to string and reset pointers
				source = source_limit = source_buffer;
				line.append(*(target = target_buffer));
			}
		}
	} while (ch >= 0);
}

void DataInputStream::readFully(byte* data, size_t offset, size_t length) throw (IOException)
{
	if (!data)
		throw NullPointerException();

	size_t total = 0;

	while (total < length)
	{
		int rc = _pin->read(data, offset+total, length-total);
		if (rc < 0)
			throw EOFException();
		total += rc;
	}
}

void DataInputStream::readFully(bytearray& b) throw (IOException)
{
	readFully(b.data(), 0, b.size());
}

off_t DataInputStream::skipBytes(off_t n) throw (IOException)
{
	off_t total = 0, rc;

	while ((total < n) && ((rc = _pin->skip(n - total)) > 0))
		total += rc;

	return total;
}
