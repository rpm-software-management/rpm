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

/*!\file SHA1Digest.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_SHA1DIGEST_H
#define _CLASS_SHA1DIGEST_H

#include "beecrypt/beecrypt.h"
#include "beecrypt/sha1.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/MessageDigestSpi.h"
using beecrypt::security::MessageDigestSpi;

namespace beecrypt {
	namespace provider {
		class SHA1Digest : public MessageDigestSpi
		{
			private:
				sha1Param _param;
				bytearray _digest;

			protected:
				virtual const bytearray& engineDigest();
				virtual size_t engineDigest(byte*, size_t, size_t) throw (ShortBufferException);
				virtual size_t engineGetDigestLength();
				virtual void engineReset();
				virtual void engineUpdate(byte);
				virtual void engineUpdate(const byte*, size_t, size_t);

			public:
				SHA1Digest();
				virtual ~SHA1Digest();

				virtual SHA1Digest* clone() const;

		};
	}
}

#endif

#endif
