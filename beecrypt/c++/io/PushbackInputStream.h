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

/*!\file PushbackInputStream.h
 * \ingroup CXX_IO_m
 */

#ifndef _CLASS_PUSHBACKINPUTSTREAM_H
#define _CLASS_PUSHBACKINPUTSTREAM_H

#ifdef __cplusplus

#include "beecrypt/c++/io/FilterInputStream.h"
using beecrypt::io::FilterInputStream;

namespace beecrypt {
	namespace io {
		class BEECRYPTCXXAPI PushbackInputStream : public FilterInputStream
		{
			private:
				bool _closed;

			protected:
				bytearray buf;
				size_t pos;

			public:
				PushbackInputStream(InputStream& in, size_t size = 1);
				virtual ~PushbackInputStream();

				virtual off_t available() throw (IOException);
				virtual void close() throw (IOException);
				virtual bool markSupported() throw ();
				virtual int read() throw (IOException);
				virtual int read(byte* data, size_t offset, size_t length) throw (IOException);
				virtual off_t skip(off_t n) throw (IOException);

				void unread(byte) throw (IOException);
				void unread(const byte* data, size_t offset, size_t length) throw (IOException);
				void unread(const bytearray& b) throw (IOException);

		};
	}
}

#endif

#endif
