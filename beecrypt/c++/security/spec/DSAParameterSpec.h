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

/*!\file DSAParameterSpec.h
 * \ingroup CXX_SECURITY_SPEC_m
 */

#ifndef _CLASS_DSAPARAMETERSPEC_H
#define _CLASS_DSAPARAMETERSPEC_H

#include "beecrypt/api.h"
#include "beecrypt/mpbarrett.h"
#include "beecrypt/dsa.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/interfaces/DSAParams.h"
using beecrypt::security::interfaces::DSAParams;
#include "beecrypt/c++/security/spec/AlgorithmParameterSpec.h"
using beecrypt::security::spec::AlgorithmParameterSpec;

namespace beecrypt {
	namespace security {
		namespace spec {
			class BEECRYPTCXXAPI DSAParameterSpec : public AlgorithmParameterSpec, public DSAParams
			{
				private:
					mpbarrett _p;
					mpbarrett _q;
					mpnumber _g;

				public:
					DSAParameterSpec(const DSAParams&);
					DSAParameterSpec(const mpbarrett& p, const mpbarrett& q, const mpnumber& g);
					virtual ~DSAParameterSpec();

					const mpbarrett& getP() const throw ();
					const mpbarrett& getQ() const throw ();
					const mpnumber& getG() const throw ();
			};
		}
	}
}

#endif

#endif
