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

#include "beecrypt/beecrypt.h"
#include "beecrypt/aes.h"
#include "beecrypt/blockmode.h"
#include "beecrypt/blockpad.h"
#include "beecrypt/hmacsha256.h"
#include "beecrypt/pkcs12.h"
#include "beecrypt/sha256.h"
#include "beecrypt/c++/provider/KeyProtector.h"
#include "beecrypt/c++/beeyond/AnyEncodedKeySpec.h"
using beecrypt::beeyond::AnyEncodedKeySpec;
#include "beecrypt/c++/crypto/BadPaddingException.h"
using beecrypt::crypto::BadPaddingException;
#include "beecrypt/c++/io/ByteArrayInputStream.h"
using beecrypt::io::ByteArrayInputStream;
#include "beecrypt/c++/io/ByteArrayOutputStream.h"
using beecrypt::io::ByteArrayOutputStream;
#include "beecrypt/c++/io/DataInputStream.h"
using beecrypt::io::DataInputStream;
#include "beecrypt/c++/io/DataOutputStream.h"
using beecrypt::io::DataOutputStream;
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/security/KeyFactory.h"
using beecrypt::security::KeyFactory;

using namespace beecrypt::provider;

namespace {
	/* eventually these will be moved to a different location */
	void pkcs5_pad(size_t blockbytes, bytearray& b)
	{
		size_t unpadded_size = b.size();

		byte padvalue = blockbytes - (unpadded_size % blockbytes);

		b.resize(unpadded_size + padvalue);

		memset(b.data() + unpadded_size, padvalue, padvalue);
	}

	void pkcs5_unpad(size_t blockbytes, bytearray& b) throw (BadPaddingException)
	{
		byte padvalue = b[b.size() - 1];

		if (padvalue > blockbytes)
			throw BadPaddingException();

		for (size_t i = (b.size() - padvalue); i < (b.size() - 1); i++)
			if (b[i] != padvalue)
				throw BadPaddingException();

		b.resize(b.size() - padvalue);
	}
}

KeyProtector::KeyProtector(PBEKey& key) throw (InvalidKeyException)
{
	bytearray _rawk, _salt;
	size_t _iter;

	if (key.getEncoded())
		_rawk = *(key.getEncoded());
	else
		throw InvalidKeyException("PBEKey must have an encoding");

	if (key.getSalt())
		_salt = *(key.getSalt());

	_iter = key.getIterationCount();

	if (pkcs12_derive_key(&sha256, PKCS12_ID_CIPHER, _rawk.data(), _rawk.size(), _salt.data(), _salt.size(), _iter, _cipher_key, 32))
        throw InvalidKeyException("pkcs12_derive_key returned error");

	if (pkcs12_derive_key(&sha256, PKCS12_ID_MAC, _rawk.data(), _rawk.size(), _salt.data(), _salt.size(), _iter, _mac_key, 32))
        throw InvalidKeyException("pkcs12_derive_key returned error");

	if (pkcs12_derive_key(&sha256, PKCS12_ID_IV, _rawk.data(), _rawk.size(), _salt.data(), _salt.size(), _iter, _iv, 16))
        throw InvalidKeyException("pkcs12_derive_key returned error");
}

KeyProtector::~KeyProtector() throw ()
{
	// wipe everything
	memset(_cipher_key, 0, 32);
	memset(_mac_key, 0, 32);
	memset(_iv, 0, 16);
}

bytearray* KeyProtector::protect(const PrivateKey& pri) throw ()
{
	if (!pri.getEncoded())
		return 0;

	if (!pri.getFormat())
		return 0;

	/* Eventually we'll substitute this with the following construction:
	 * DataOutputStream(CipherOutputStream(ByteArrayOutputStream)))
	 */
	ByteArrayOutputStream bos;
	DataOutputStream dos(bos);

	try
	{
		const bytearray* encoded_key = pri.getEncoded();

		dos.writeUTF(pri.getAlgorithm());
		dos.writeUTF(*pri.getFormat());
		dos.writeInt(encoded_key->size());
		dos.write(*encoded_key);
		dos.close();

		bytearray cleartext, ciphertext, mac(hmacsha256.digestsize);	
		bos.toByteArray(cleartext);

		// Compute the MAC before padding
		keyedHashFunctionContext mc(&hmacsha256);
		keyedHashFunctionContextSetup(&mc, _mac_key, 256);
		keyedHashFunctionContextUpdate(&mc, cleartext.data(), cleartext.size());
		keyedHashFunctionContextDigest(&mc, mac.data());

		// Pad the cleartext
		pkcs5_pad(aes.blocksize, cleartext);

		// Set the ciphertext size equal to the cleartext size
		ciphertext.resize(cleartext.size());

		// Encrypt the cleartext
		blockCipherContext bc(&aes);
		blockCipherContextSetup(&bc, _cipher_key, 256, ENCRYPT);
		blockCipherContextSetIV(&bc, _iv);
		blockCipherContextCBC(&bc, (uint32_t*) ciphertext.data(), (const uint32_t*) cleartext.data(), cleartext.size() / 16);

		// Return the concatenation of the two bytearrays
		return new bytearray(ciphertext + mac);
	}
	catch (IOException)
	{
	}
	
	return 0;
}

PrivateKey* KeyProtector::recover(const byte* data, size_t size) throw (NoSuchAlgorithmException, UnrecoverableKeyException)
{
	// If we don't have at least enough data for the digest then bail out
	if (size <= hmacsha256.digestsize)
		throw UnrecoverableKeyException("encrypted key data way too short");

	size_t ciphertext_size = size - hmacsha256.digestsize;

	// Check if we have a whole number of blocks in the data
	if ((ciphertext_size % aes.blocksize) != 0)
		throw UnrecoverableKeyException("encrypted key data is not a whole number of blocks");

	bytearray ciphertext(data, ciphertext_size), cleartext(ciphertext_size);

	// Decrypt the ciphertext
	blockCipherContext bc(&aes);
	blockCipherContextSetup(&bc, _cipher_key, 256, DECRYPT);
	blockCipherContextSetIV(&bc, _iv);
	blockCipherContextCBC(&bc, (uint32_t*) cleartext.data(), (const uint32_t*) ciphertext.data(), ciphertext_size / 16);

	try
	{
		pkcs5_unpad(aes.blocksize, cleartext);
	}
	catch (BadPaddingException)
	{
		// Corrupted data, most likely due to bad password
		throw UnrecoverableKeyException("bad padding");
	}

	bytearray mac(hmacsha256.digestsize);

	// Verify the MAC before recovering the key
	keyedHashFunctionContext mc(&hmacsha256);
	keyedHashFunctionContextSetup(&mc, _mac_key, 256);
	keyedHashFunctionContextUpdate(&mc, cleartext.data(), cleartext.size());
	keyedHashFunctionContextDigest(&mc, mac.data());

	// Compare the two MACs and bail out if they're different
	if (memcmp(data + ciphertext_size, mac.data(), hmacsha256.digestsize))
		return 0;

	// Now we're sure the password was correct, and we have the decrypted data

	ByteArrayInputStream bis(cleartext);
	DataInputStream dis(bis);

	try
	{
		String algorithm, format;
		bytearray enc;

		dis.readUTF(algorithm);
		dis.readUTF(format);

		javaint encsize = dis.readInt();
		if (encsize <= 0)
			throw IOException();

		enc.resize(encsize);

		dis.readFully(enc);

		AnyEncodedKeySpec spec(format, enc);
		KeyFactory* kf;
		PrivateKey* pri;

		try
		{
			kf = KeyFactory::getInstance(algorithm);
			pri = kf->generatePrivate(spec);

			delete kf;

			return pri;
		}
		catch (InvalidKeySpecException)
		{
			delete kf;
		}
		catch (NoSuchAlgorithmException)
		{
		}
	}
	catch (IOException)
	{
	}
	throw UnrecoverableKeyException("parsing error in decrypted key");
}

PrivateKey* KeyProtector::recover(const bytearray& b) throw (NoSuchAlgorithmException, UnrecoverableKeyException)
{
	return recover(b.data(), b.size());
}
