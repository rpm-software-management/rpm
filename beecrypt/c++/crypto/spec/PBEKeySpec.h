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

/*!\file PBEKeySpec.h
 * \ingroup CXX_CRYPTO_SPEC_m
 */

#ifndef _CLASS_PBEKEYSPEC_H
#define _CLASS_PBEKEYSPEC_H

#ifdef __cplusplus

#include "beecrypt/c++/array.h"
using beecrypt::array;
using beecrypt::bytearray;
#include "beecrypt/c++/security/spec/KeySpec.h"
using beecrypt::security::spec::KeySpec;

namespace beecrypt {
	namespace crypto {
		namespace spec {
			class BEECRYPTCXXAPI PBEKeySpec : public KeySpec
			{
				private:
					array<javachar> _password;
					bytearray* _salt;
					size_t _iteration_count;
					size_t _key_length;

				public:
					PBEKeySpec(const array<javachar>* password);
					PBEKeySpec(const array<javachar>* password, const bytearray* salt, size_t iterationCount, size_t keyLength);
					virtual ~PBEKeySpec();

					const array<javachar>& getPassword() const throw ();
					const bytearray* getSalt() const throw ();
					size_t getIterationCount() const throw ();
					size_t getKeyLength() const throw ();
			};
		}
	}
}

#endif

#endif
