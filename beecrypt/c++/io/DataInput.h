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

/*!\file DataInput.h
 * \ingroup CXX_IO_m
 */

#ifndef _INTERFACE_DATAINPUT_H
#define _INTERFACE_DATAINPUT_H

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::bytearray;
#include "beecrypt/c++/io/IOException.h"
using beecrypt::io::IOException;

namespace beecrypt {
	namespace io {
		class DataInput
		{
			public:
				virtual bool readBoolean() throw (IOException) = 0;
				virtual javabyte readByte() throw (IOException) = 0;
				virtual javachar readChar() throw (IOException) = 0;
				virtual void readFully(byte*, size_t, size_t) = 0;
				virtual void readFully(bytearray&) = 0;
				virtual javaint readInt() throw (IOException) = 0;
				virtual String* readLine() throw (IOException) = 0;
				virtual void readLine(String&) throw (IOException) = 0;
				virtual javalong readLong() throw (IOException) = 0;
				virtual javashort readShort() throw (IOException) = 0;
				virtual int readUnsignedByte() throw (IOException) = 0;
				virtual int readUnsignedShort() throw (IOException) = 0;
				virtual String* readUTF() throw (IOException) = 0;
				virtual void readUTF(String&) throw (IOException) = 0;
				virtual off_t skipBytes(off_t n) throw (IOException) = 0;
		};
	}
}

#endif

#endif
