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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/beeyond/PKCS12PBEKey.h"
using beecrypt::beeyond::PKCS12PBEKey;
#include "beecrypt/c++/crypto/spec/PBEKeySpec.h"
using beecrypt::crypto::spec::PBEKeySpec;
#include "beecrypt/c++/provider/PKCS12KeyFactory.h"

using namespace beecrypt::provider;

PKCS12KeyFactory::PKCS12KeyFactory()
{
}

PKCS12KeyFactory::~PKCS12KeyFactory()
{
}

SecretKey* PKCS12KeyFactory::engineGenerateSecret(const KeySpec& spec) throw (InvalidKeySpecException)
{
	const PBEKeySpec* pbe = dynamic_cast<const PBEKeySpec*>(&spec);
	if (pbe)
	{
		return new PKCS12PBEKey(pbe->getPassword(), pbe->getSalt(), pbe->getIterationCount());
	}
	throw InvalidKeySpecException("Expected a PBEKeySpec");
}

KeySpec* PKCS12KeyFactory::engineGetKeySpec(const SecretKey& key, const type_info& info) throw (InvalidKeySpecException)
{
	const PBEKey* pbe = dynamic_cast<const PBEKey*>(&key);
	if (pbe)
	{
		if (info == typeid(KeySpec) || info == typeid(PBEKeySpec))
		{
			return new PBEKeySpec(&pbe->getPassword(), pbe->getSalt(), pbe->getIterationCount(), 0);
		}
		throw InvalidKeySpecException("Unsupported KeySpec type");
	}
	throw InvalidKeySpecException("Unsupported SecretKey type");
}

SecretKey* PKCS12KeyFactory::engineTranslateKey(const SecretKey& key) throw (InvalidKeyException)
{
	const PBEKey* pbe = dynamic_cast<const PBEKey*>(&key);
	if (pbe)
	{
		return new PKCS12PBEKey(pbe->getPassword(), pbe->getSalt(), pbe->getIterationCount());
	}
	throw InvalidKeyException("Unsupported SecretKey type");
}
