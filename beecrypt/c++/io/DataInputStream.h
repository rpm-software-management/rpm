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

/*!\file DataInputStream.h
 * \ingroup CXX_IO_m
 */

#ifndef _CLASS_DATAINPUTSTREAM_H
#define _CLASS_DATAINPUTSTREAM_H

#ifdef __cplusplus

#include "beecrypt/c++/io/DataInput.h"
using beecrypt::io::DataInput;
#include "beecrypt/c++/io/FilterInputStream.h"
using beecrypt::io::FilterInputStream;

#include <unicode/ucnv.h>

namespace beecrypt {
	namespace io {
		class BEECRYPTCXXAPI DataInputStream : public FilterInputStream, public DataInput
		{
			private:
				bool _del;
				InputStream* _pin;
				UConverter* _utf;
				UConverter* _loc;

			public:
				DataInputStream(InputStream& in);
				virtual ~DataInputStream();

				virtual bool readBoolean() throw (IOException);
				virtual javabyte readByte() throw (IOException);
				virtual javachar readChar() throw (IOException);
				virtual void readFully(byte* data, size_t offset, size_t length) throw (IOException);
				virtual void readFully(bytearray& b) throw (IOException);
				virtual javaint readInt() throw (IOException);
				virtual String* readLine() throw (IOException);
				virtual void readLine(String&) throw (IOException);
				virtual javalong readLong() throw (IOException);
				virtual javashort readShort() throw (IOException);
				virtual int readUnsignedByte() throw (IOException);
				virtual int readUnsignedShort() throw (IOException);
				virtual String* readUTF() throw (IOException);
				virtual void readUTF(String&) throw (IOException);
				virtual off_t skipBytes(off_t n) throw (IOException);
		};
	}
}

#endif

#endif
