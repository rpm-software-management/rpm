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

#include "beecrypt/c++/security/Security.h"
using beecrypt::security::Security;
#include "beecrypt/c++/security/AlgorithmParameterGenerator.h"
using beecrypt::security::AlgorithmParameterGenerator;
#include "beecrypt/c++/security/AlgorithmParameters.h"
using beecrypt::security::AlgorithmParameters;
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;
#include "beecrypt/c++/security/KeyPairGenerator.h"
using beecrypt::security::KeyPairGenerator;
#include "beecrypt/c++/security/Signature.h"
using beecrypt::security::Signature;
#include "beecrypt/c++/security/spec/EncodedKeySpec.h"
using beecrypt::security::spec::EncodedKeySpec;

#include <iostream>
using namespace std;
#include <unicode/ustream.h>

int main(int argc, char* argv[])
{
   int failures = 0;

	try
	{
		KeyPairGenerator* kpg = KeyPairGenerator::getInstance("RSA");

		kpg->initialize(1024);

		KeyPair* pair = kpg->generateKeyPair();

		Signature* sig = Signature::getInstance("SHA1withRSA");

		sig->initSign(pair->getPrivate());

		bytearray* tmp = sig->sign();

		sig->initVerify(pair->getPublic());

		if (!sig->verify(*tmp))
			failures++;

		KeyFactory* kf = KeyFactory::getInstance("BEE");

		KeySpec* spec = kf->getKeySpec(pair->getPublic(), typeid(EncodedKeySpec));

		PublicKey* pub = kf->generatePublic(*spec);

		delete pub;
		delete spec;
		delete kf;
		delete tmp;
		delete sig;
		delete pair;
		delete kpg;
	}
	catch (Exception& ex)
	{
		cerr << "Exception: " << ex.getMessage() << endl;
		failures++;
	}
	catch (...)
	{
		cerr << "exception" << endl;
		failures++;
	}
	return failures;
}
