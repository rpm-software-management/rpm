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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/c++/security/Signature.h"
#include "beecrypt/c++/security/Security.h"

using namespace beecrypt::security;

Signature::Signature(SignatureSpi* spi, const String& algorithm, const Provider& provider)
{
	_sspi = spi;
	_algo = algorithm;
	_prov = &provider;
}

Signature::~Signature()
{
	delete _sspi;
}

Signature* Signature::getInstance(const String& algorithm) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "Signature");

	Signature* result = new Signature((SignatureSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

Signature* Signature::getInstance(const String& algorithm, const String& provider) throw (NoSuchAlgorithmException, NoSuchProviderException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "Signature", provider);

	Signature* result = new Signature((SignatureSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

Signature* Signature::getInstance(const String& algorithm, const Provider& provider) throw (NoSuchAlgorithmException)
{
	Security::spi* tmp = Security::getSpi(algorithm, "Signature", provider);

	Signature* result = new Signature((SignatureSpi*) tmp->cspi, tmp->name, tmp->prov);

	delete tmp;

	return result;
}

AlgorithmParameters* Signature::getParameters() const
{
	return _sspi->engineGetParameters();
}

void Signature::setParameter(const AlgorithmParameterSpec& spec) throw (InvalidAlgorithmParameterException)
{
	_sspi->engineSetParameter(spec);
}

void Signature::initSign(const PrivateKey& key) throw (InvalidKeyException)
{
	_sspi->engineInitSign(key, (SecureRandom*) 0);

	state = SIGN;
}

void Signature::initSign(const PrivateKey& key, SecureRandom* random) throw (InvalidKeyException)
{
	_sspi->engineInitSign(key, random);

	state = SIGN;
}

void Signature::initVerify(const PublicKey& key) throw (InvalidKeyException)
{
	_sspi->engineInitVerify(key);

	state = VERIFY;
}

bytearray* Signature::sign() throw (IllegalStateException, SignatureException)
{
	if (state != SIGN)
		throw IllegalStateException("object not initialized for signing");

	return _sspi->engineSign();
}

size_t Signature::sign(byte* outbuf, size_t offset, size_t len) throw (ShortBufferException, IllegalStateException, SignatureException)
{
	if (state != SIGN)
		throw IllegalStateException("object not initialized for signing");

	return _sspi->engineSign(outbuf, offset, len);
}

size_t Signature::sign(bytearray& out) throw (IllegalStateException, SignatureException)
{
	if (state != SIGN)
		throw IllegalStateException("object not initialized for signing");

	return _sspi->engineSign(out);
}

bool Signature::verify(const bytearray& signature) throw (IllegalStateException, SignatureException)
{
	return verify(signature.data(), 0, signature.size());
}

bool Signature::verify(const byte* signature, size_t offset, size_t len) throw (IllegalStateException, SignatureException)
{
	if (state != VERIFY)
		throw IllegalStateException("object not initialized for verification");

	return _sspi->engineVerify(signature, offset, len);
}

void Signature::update(byte b) throw (IllegalStateException)
{
	if (state == UNINITIALIZED)
		throw IllegalStateException("object not initialized for signing or verification");

	_sspi->engineUpdate(b);
}

void Signature::update(const byte* data, size_t offset, size_t len) throw (IllegalStateException)
{
	if (state == UNINITIALIZED)
		throw IllegalStateException("object not initialized for signing or verification");

	_sspi->engineUpdate(data, offset, len);
}

void Signature::update(const bytearray& b) throw (IllegalStateException)
{
	update(b.data(), 0, b.size());
}

const String& Signature::getAlgorithm() const throw ()
{
	return _algo;
}

const Provider& Signature::getProvider() const throw ()
{
	return *_prov;
}
