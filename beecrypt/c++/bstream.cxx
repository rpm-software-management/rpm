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

#define BEECRYPT_CXX_DLL_EXPORT

#include "beecrypt/c++/bstream.h"

#include "beecrypt/c++/crypto/interfaces/DHPublicKey.h"
using beecrypt::crypto::interfaces::DHPublicKey;
#include "beecrypt/c++/security/interfaces/DSAPublicKey.h"
using beecrypt::security::interfaces::DSAPublicKey;
#include "beecrypt/c++/security/interfaces/RSAPublicKey.h"
using beecrypt::security::interfaces::RSAPublicKey;

#include <unicode/ustream.h>

using namespace beecrypt;

ostream& operator<<(ostream& stream, const PublicKey& pub)
{
	stream << pub.getAlgorithm() << " public key" << endl;

	const DHPublicKey* dh = dynamic_cast<const DHPublicKey*>(&pub);
	if (dh)
	{
		return stream << "P = " << dh->getParams().getP() << endl <<
		                 "G = " << dh->getParams().getG() << endl <<
		                 "Y = " << dh->getY() << endl;
	}

	const DSAPublicKey* dsa = dynamic_cast<const DSAPublicKey*>(&pub);
	if (dsa)
	{
		return stream << "P = " << dsa->getParams().getP() << endl <<
		                 "Q = " << dsa->getParams().getQ() << endl <<
		                 "G = " << dsa->getParams().getG() << endl <<
		                 "Y = " << dsa->getY() << endl;
	}

	const RSAPublicKey* rsa = dynamic_cast<const RSAPublicKey*>(&pub);
	if (rsa)
	{
		return stream << "N = " << rsa->getModulus() << endl <<
		                 "E = " << rsa->getPublicExponent() << endl;
	}

	return stream;
}
