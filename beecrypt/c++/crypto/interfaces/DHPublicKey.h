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

/*!\file DHPublicKey.h
 * \ingroup CXX_CRYPTO_INTERFACES_m
 */

#ifndef _INTERFACE_DHPUBLICKEY_H
#define _INTERFACE_DHPUBLICKEY_H

#include "beecrypt/api.h"
#include "beecrypt/mpnumber.h"

#ifdef __cplusplus

#include "beecrypt/c++/security/PublicKey.h"
using beecrypt::security::PublicKey;
#include "beecrypt/c++/crypto/interfaces/DHKey.h"
using beecrypt::crypto::interfaces::DHKey;

namespace beecrypt {
	namespace crypto {
		namespace interfaces {
			/*!\brief Diffie-Hellman PublicKey interface
			* \ingroup CXX_IF_m
			*/
			class DHPublicKey : public PublicKey, public DHKey
			{
				public:
					virtual const mpnumber& getY() const throw () = 0;
			};
		}
	}
}

#endif

#endif
