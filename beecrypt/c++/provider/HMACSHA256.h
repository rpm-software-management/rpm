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

/*!\file HMACSHA256.h
 * \ingroup CXX_PROV_m
 */

#ifndef _CLASS_HMACSHA256_H
#define _CLASS_HMACSHA256_H

#include "beecrypt/beecrypt.h"
#include "beecrypt/hmacsha256.h"

#ifdef __cplusplus

#include "beecrypt/c++/crypto/MacSpi.h"
using beecrypt::crypto::MacSpi;

namespace beecrypt {
	namespace provider {
		class HMACSHA256 : public MacSpi
		{
			private:
				hmacsha256Param _param;
				bytearray _digest;

			protected:
				virtual const bytearray& engineDoFinal();
				virtual size_t engineDoFinal(byte*, size_t, size_t) throw (ShortBufferException);
				virtual size_t engineGetMacLength();
				virtual void engineInit(const Key&, const AlgorithmParameterSpec* spec) throw (InvalidKeyException, InvalidAlgorithmParameterException);
				virtual void engineReset();
				virtual void engineUpdate(byte);
				virtual void engineUpdate(const byte*, size_t, size_t);

			public:
				HMACSHA256();
				virtual ~HMACSHA256();

				virtual HMACSHA256* clone() const;
		};
	}
}

#endif

#endif
