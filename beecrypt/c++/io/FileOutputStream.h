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

/*!\file FileOutputStream.h
 * \ingroup CXX_IO_m
 */

#ifndef _CLASS_FILEOUTPUTSTREAM_H
#define _CLASS_FILEOUTPUTSTREAM_H

#ifdef __cplusplus

#include "beecrypt/c++/io/OutputStream.h"
using beecrypt::io::OutputStream;

namespace beecrypt {
	namespace io {
		class BEECRYPTCXXAPI FileOutputStream : public OutputStream
		{
			private:
				FILE* _f;

			public:
				FileOutputStream(FILE* f);
				virtual ~FileOutputStream();

				virtual void close() throw (IOException);
				virtual void flush() throw (IOException);
				virtual void write(byte b) throw (IOException);
				virtual void write(const byte* data, size_t offset, size_t length) throw (IOException);
				virtual void write(const bytearray&) throw (IOException);
		};
	}
}

#endif

#endif
