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

/*!\file ByteArrayOutputStream.h
 * \ingroup CXX_IO_m
 */

#ifndef _CLASS_BYTEARRAYOUTPUTSTREAM_H
#define _CLASS_BYTEARRAYOUTPUTSTREAM_H

#ifdef __cplusplus

#include "beecrypt/c++/mutex.h"
using beecrypt::mutex;
#include "beecrypt/c++/io/OutputStream.h"
using beecrypt::io::OutputStream;

namespace beecrypt {
	namespace io {
		class BEECRYPTCXXAPI ByteArrayOutputStream : public OutputStream
		{
			private:
				mutex _lock;

			protected:
				bytearray _buf;
				size_t    _count;

			public:
				ByteArrayOutputStream();
				ByteArrayOutputStream(size_t);
				virtual ~ByteArrayOutputStream();

				void reset() throw ();
				size_t size() throw ();
				bytearray* toByteArray();
				void toByteArray(bytearray&);
				void writeTo(OutputStream& out) throw (IOException);

				virtual void close() throw (IOException);
				virtual void flush() throw (IOException);
				virtual void write(byte b) throw (IOException);
				virtual void write(const byte* data, size_t offset, size_t length) throw (IOException);
				virtual void write(const bytearray& b) throw (IOException);
		};
	}
}

#endif

#endif
