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

/*!\file RSAKeyGenParameterSpec.h
 * \ingroup CXX_SECURITY_SPEC_m
 */

#ifndef _CLASS_RSAKEYGENPARAMETERSPEC
#define _CLASS_RSAKEYGENPARAMETERSPEC

#include "beecrypt/beecrypt.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace security {
		namespace spec {
			class BEECRYPTCXXAPI RSAKeyGenParameterSpec : public AlgorithmParameterSpec
			{
				public:
					static const mpnumber F0;
					static const mpnumber F4;

				private:
					size_t _keysize;
					mpnumber _e;
				
				public:
					RSAKeyGenParameterSpec(size_t, const mpnumber&);
					virtual ~RSAKeyGenParameterSpec();

					size_t getKeysize() const throw ();
					const mpnumber& getPublicExponent() const throw ();
			};
		}
	}
}

#endif

#endif
