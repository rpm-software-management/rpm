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

/*!\file KeyPair.h
 * \ingroup CXX_SECURITY_m
 */

#ifndef _CLASS_KEYPAIR_H
#define _CLASS_KEYPAIR_H

#ifdef __cplusplus

#include "beecrypt/c++/security/PrivateKey.h"
using beecrypt::security::PrivateKey;
#include "beecrypt/c++/security/PublicKey.h"
using beecrypt::security::PublicKey;

namespace beecrypt {
	namespace security {
		class BEECRYPTCXXAPI KeyPair
		{
			friend class KeyPairGenerator;

			private:
				PublicKey* pub;
				PrivateKey* pri;

			public:
				KeyPair(const PublicKey&, const PrivateKey&);
				KeyPair(PublicKey*, PrivateKey*);
				~KeyPair();

				const PublicKey& getPublic() const throw ();
				const PrivateKey& getPrivate() const throw ();
		};
	}
}

#endif

#endif
