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

/*!\file DHParameterSpec.h
 * \ingroup CXX_CRYPTO_SPEC_m
 */

#ifndef _CLASS_DHPARAMETERSPEC_H
#define _CLASS_DHPARAMETERSPEC_H

#include "beecrypt/api.h"
#include "beecrypt/mpbarrett.h"
#include "beecrypt/dlsvdp-dh.h"

#ifdef __cplusplus

#include "beecrypt/c++/crypto/interfaces/DHParams.h"
using beecrypt::crypto::interfaces::DHParams;
#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace crypto {
		namespace spec {
			class BEECRYPTCXXAPI DHParameterSpec : public AlgorithmParameterSpec, public DHParams
			{
				private:
					mpbarrett _p;
					mpnumber _g;
					size_t  _l;

				public:
					DHParameterSpec(const DHParams&);
					DHParameterSpec(const mpbarrett& p, const mpnumber& g);
					DHParameterSpec(const mpbarrett& p, const mpnumber& g, size_t l);
					virtual ~DHParameterSpec();

					const mpbarrett& getP() const throw ();
					const mpnumber& getG() const throw ();
					size_t getL() const throw ();
			};
		}
	}
}

#endif

#endif
