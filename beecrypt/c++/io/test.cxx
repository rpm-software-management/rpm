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

#include "c++/io/ByteArrayInputStream.h"
using beecrypt::io::ByteArrayInputStream;
#include "c++/io/ByteArrayOutputStream.h"
using beecrypt::io::ByteArrayOutputStream;
#include "c++/io/DataInputStream.h"
using beecrypt::io::DataInputStream;
#include "c++/io/DataOutputStream.h"
using beecrypt::io::DataOutputStream;

#include <iostream>
#include <unicode/ustream.h>
using namespace std;

int main(int argc, char* argv[])
{
	String input = UNICODE_STRING_SIMPLE("The quick brown fox jumps over the lazy dog");

	int failures = 0;

	try
	{
		ByteArrayOutputStream bos;
		DataOutputStream dos(bos);

		dos.writeUTF(input);
		dos.close();

		bytearray* b = bos.toByteArray();

		if (b)
		{
			if (b->size() != 45)
			{
				cerr << "failed test 1" << endl;
				failures++;
			}

			ByteArrayInputStream bin(*b);
			DataInputStream din(bin);

			String test;

			din.readUTF(test);

			if (input != test)
			{
				cerr << "failed test 2" << endl;
				failures++;
			}

			if (din.available() != 0)
			{
				cerr << "failed test 3" << endl;
				cerr << "remaining bytes in stream: " << din.available() << endl;
				failures++;
			}

			din.close();
			bin.close();
		}
		else
		{
			cerr << "failed structural 1" << endl;
			failures++;
		}
	}
	catch (IOException& ex)
	{
		cerr << "failed structural 2" << endl;
		failures++;
	}
	catch (...)
	{
		cerr << "failed structural 3" << endl;
		failures++;
	}

	return failures;
}
