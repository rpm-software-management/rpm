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

/*!\file MacOutputStream.h
 * \ingroup CXX_CRYPTO_m
 */

#ifndef _CLASS_MACOUTPUTSTREAM_H
#define _CLASS_MACOUTPUTSTREAM_H

#ifdef __cplusplus

#include "beecrypt/c++/crypto/Mac.h"
using beecrypt::crypto::Mac;
#include "beecrypt/c++/io/FilterOutputStream.h"
using beecrypt::io::FilterOutputStream;

namespace beecrypt {
	namespace crypto {
		class BEECRYPTCXXAPI MacOutputStream : public FilterOutputStream
		{
			private:
				bool _on;

			protected:
				Mac& mac;

			public:
				MacOutputStream(OutputStream&, Mac&);
				virtual ~MacOutputStream();

				virtual void write(byte) throw (IOException);
				virtual void write(const byte* data, size_t offset, size_t length) throw (IOException);

				void on(bool);

				Mac& getMac();
				void setMac(Mac&);

		};
	}
}

#endif

#endif

