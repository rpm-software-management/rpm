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

#include "beecrypt/c++/io/PrintStream.h"
#include "beecrypt/c++/lang/IllegalArgumentException.h"
using beecrypt::lang::IllegalArgumentException;

#define MAX_BYTES_PER_CHARACTER	8

using namespace beecrypt::io;

PrintStream::PrintStream(OutputStream& out, bool autoflush, const char* encoding) : FilterOutputStream(out)
{
	UErrorCode status = U_ZERO_ERROR;

	_loc = ucnv_open(encoding, &status);
	if (U_FAILURE(status))
		throw IllegalArgumentException("invalid encoding");

	_closed = false;
	_error = false;
	_flush = autoflush;
}

PrintStream::~PrintStream()
{
	ucnv_close(_loc);
}

void PrintStream::close() throw ()
{
	try
	{
		out.close();
		_closed = true;
	}
	catch (IOException)
	{
		_error = true;
	}
}

void PrintStream::flush() throw ()
{
	if (!_closed)
	{
		try
		{
			out.flush();
		}
		catch (IOException)
		{
			_error = true;
		}
	}
}

void PrintStream::write(byte b) throw ()
{
	if (!_closed)
	{
		try
		{
			out.write(b);
		}
		catch (IOException)
		{
			_error = true;
		}
	}
}

void PrintStream::write(const byte* data, size_t offset, size_t length) throw ()
{
	if (!_closed)
	{
		try
		{
			out.write(data, offset, length);
		}
		catch (IOException)
		{
			_error = true;
		}
	}
}

void PrintStream::print(const UChar* str, size_t length) throw ()
{
	if (!_closed)
	{
		try
		{
			UErrorCode status = U_ZERO_ERROR;

			// pre-flighting
			size_t need = ucnv_fromUChars(_loc, 0, 0, str, length, &status);
			if (U_FAILURE(status))
				if (status != U_BUFFER_OVERFLOW_ERROR)
					throw IOException();

			byte* buffer = new byte[need];

			status = U_ZERO_ERROR;

			try
			{
				ucnv_fromUChars(_loc, (char*) buffer, need, str, length, &status);
				if (status != U_STRING_NOT_TERMINATED_WARNING)
					throw IOException();

				out.write(buffer, 0, need);

				if (_flush)
				{
					for (size_t i = 0; i < length; i++)
						if (str[i] == 0xA)
							out.flush();
				}

				delete[] buffer;
			}
			catch (IOException)
			{
				delete[] buffer;
				throw;
			}
		}
		catch (IOException)
		{
			_error = true;
		}
	}
}

void PrintStream::print(bool b) throw ()
{
	static const String* STR_TRUE = 0;
	static const String* STR_FALSE = 0;

	if (!_closed)
	{
		if (b)
		{
			if (!STR_FALSE)
				STR_FALSE = new String("true");

			print(*STR_TRUE);
		}
		else
		{
			if (!STR_FALSE)
				STR_FALSE = new String("false");

			print(*STR_FALSE);
		}
	}
}

void PrintStream::print(javachar ch) throw ()
{
	if (!_closed)
	{
		char buffer[MAX_BYTES_PER_CHARACTER];

		try	
		{
			UErrorCode status = U_ZERO_ERROR;

			// do conversion of one character
			size_t used = ucnv_fromUChars(_loc, buffer, 8, &ch, 1, &status);
			if (U_FAILURE(status))
				throw IOException("failure in ucnv_fromUChars");

			out.write((const byte*) buffer, 0, used);

			// check if we need to flush
			if (_flush && ch == 0xA)
				out.flush();
		}
		catch (IOException)
		{
			_error = true;
		}
	}
}

void PrintStream::print(const array<javachar>& chars) throw ()
{
	print(chars.data(), chars.size());
}

void PrintStream::print(const String& str) throw ()
{
	print(str.getBuffer(), str.length());
}

void PrintStream::println() throw ()
{
	if (!_closed)
	{
		#if WIN32
		print((javachar) 0xD);
		print((javachar) 0xA);
		#else
		print((javachar) 0xA);
		#endif
	}
}

void PrintStream::println(bool b) throw ()
{
	if (!_closed)
	{
		print(b);
		println();
	}
}

void PrintStream::println(const array<javachar>& chars) throw ()
{
	if (!_closed)
	{
		print(chars);
		println();
	}
}

void PrintStream::println(const String& str) throw ()
{
	if (!_closed)
	{
		print(str);
		println();
	}
}
