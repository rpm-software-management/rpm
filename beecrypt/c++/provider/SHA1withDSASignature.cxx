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

#include "beecrypt/c++/adapter.h"
#include "beecrypt/c++/lang/NullPointerException.h"
using beecrypt::lang::NullPointerException;
#include "beecrypt/c++/provider/SHA1withDSASignature.h"
#include "beecrypt/c++/security/interfaces/DSAPrivateKey.h"
using beecrypt::security::interfaces::DSAPrivateKey;
#include "beecrypt/c++/security/interfaces/DSAPublicKey.h"
using beecrypt::security::interfaces::DSAPublicKey;

namespace {
	const byte TAG_SEQUENCE = 0x30;
	const byte TAG_INTEGER = 0x02;

	typedef int asn1error;

	const asn1error DER_NOT_ENOUGH_DATA = -1;
	const asn1error DER_IMPLICIT_TAG_LENGTH = -2;
	const asn1error DER_TAG_TOO_LONG = -3;
	const asn1error DER_FORMAT_ERROR = -4;
	const asn1error DER_CONVERSION_ERROR = -5;

	/* compute the size of a DER length encoding */
	size_t asn1_der_length(size_t length) throw ()
	{
		if (length < 0x80)
			return 1;
		if (length < 0x100)
			return 2;
		if (length < 0x10000)
			return 3;
		if (length < 0x1000000)
			return 4;
		else
			return 5;
	}

	size_t asn1_der_length_of(const mpnumber& n) throw ()
	{
		size_t sigbits = mpbits(n.size, n.data);

		return ((sigbits + 7) >> 3) + (((sigbits & 7) == 0) ? 1 : 0);
	}

	size_t asn1_der_length_of_rssig(const mpnumber& r, const mpnumber& s) throw ()
	{
		size_t intlen, seqlen = 0;

		intlen = asn1_der_length_of(r);

		seqlen += 1 + asn1_der_length(intlen) + intlen;

		intlen = asn1_der_length_of(s);

		seqlen += 1 + asn1_der_length(intlen) + intlen;

		return 1 + asn1_der_length(seqlen) + seqlen;
	}

	size_t asn1_der_encode_length(byte* data, size_t length) throw ()
	{
		if (length < 0x80)
		{
			data[0] = (byte) length;
			return 1;
		}
		else if (length < 0x100)
		{
			data[0] = (byte) 0x81;
			data[1] = (byte) length;
			return 2;
		}
		else if (length < 0x10000)
		{
			data[0] = (byte) 0x82;
			data[1] = (byte) (length >> 8);
			data[2] = (byte) (length     );
			return 3;
		}
		else if (length < 0x1000000)
		{
			data[0] = (byte) 0x83;
			data[1] = (byte) (length >> 16);
			data[2] = (byte) (length >>  8);
			data[3] = (byte) (length      );
			return 4;
		}
		else
		{
			data[0] = (byte) 0x84;
			data[1] = (byte) (length >> 24);
			data[2] = (byte) (length >> 16);
			data[3] = (byte) (length >>  8);
			data[4] = (byte) (length      );
			return 5;
		}
	}

	size_t asn1_der_decode_length(const byte* data, size_t size, size_t* length) throw (asn1error)
	{
		size_t length_bytes;
		byte tmp;

		if (size == 0)
			throw DER_NOT_ENOUGH_DATA;

		tmp = *(data++);

		if (tmp < 0x80)
		{
			*length = tmp;
			length_bytes = 0;
		}
		else
		{
			byte length_bytes = tmp & 0x7f;

			if (length_bytes == 0)
				throw DER_IMPLICIT_TAG_LENGTH;
																					
			if (length_bytes >= size)
				throw DER_NOT_ENOUGH_DATA;
				
			if (length_bytes > sizeof(size_t))
				throw DER_TAG_TOO_LONG;

			size_t temp = 0;
	                                                                                                    
			for (byte i = 0; i < length_bytes; i++)
			{
				tmp = *(data++);
				temp <<= 8;
				temp += tmp;
			}

			*length = temp;
		}
		return 1 + length_bytes;
	}

	size_t asn1_der_encode(byte* data, const mpnumber& n) throw ()
	{
		size_t offset = 1, length = asn1_der_length_of(n);

		data[0] = TAG_INTEGER;

		offset += asn1_der_encode_length(data+offset, length);

		i2osp(data+offset, length, n.data, n.size);

		offset += length;

		return offset;
	}

	size_t asn1_der_decode(const byte* data, size_t size, mpnumber& n) throw (asn1error)
	{
		size_t length, offset = 1;

		if (size < 2)
			throw DER_NOT_ENOUGH_DATA;

		if (data[0] != TAG_INTEGER)
			throw DER_FORMAT_ERROR;

		offset += asn1_der_decode_length(data+offset, size-offset, &length);

		if (length > (size-offset))
			throw DER_NOT_ENOUGH_DATA;

		if (mpnsetbin(&n, data+offset, length))
			throw DER_CONVERSION_ERROR;

		offset += length;

		return offset;
	}

	size_t asn1_der_encode_rssig(byte* data, const mpnumber& r, const mpnumber& s) throw ()
	{
		size_t intlen, seqlen = 0;

		intlen = asn1_der_length_of(r);
		seqlen += 1 + asn1_der_length(intlen) + intlen;
		intlen = asn1_der_length_of(s);
		seqlen += 1 + asn1_der_length(intlen) + intlen;

		*(data++) = TAG_SEQUENCE;

		data += asn1_der_encode_length(data, seqlen);
		data += asn1_der_encode(data, r);
		data += asn1_der_encode(data, s);

		return 1 + asn1_der_length(seqlen) + seqlen;
	}

	size_t asn1_der_decode_rssig(const byte* data, size_t size, mpnumber& r, mpnumber& s) throw (asn1error)
	{
		size_t tmp, length, offset = 1;

		if (size < 2)
			throw DER_NOT_ENOUGH_DATA;

		if (data[0] != TAG_SEQUENCE)
			throw DER_FORMAT_ERROR;

		offset += asn1_der_decode_length(data+offset, size-offset, &length);

		if (length > (size-offset))
			throw DER_NOT_ENOUGH_DATA;

		tmp = asn1_der_decode(data+offset, length, r);

		offset += tmp;
		length -= tmp;

		tmp = asn1_der_decode(data+offset, length, s);

		offset += tmp;
		length -= tmp;

		if (length > 0)
			throw DER_FORMAT_ERROR;

		return offset;
	}
}

using namespace beecrypt::provider;

SHA1withDSASignature::SHA1withDSASignature()
{
}

SHA1withDSASignature::~SHA1withDSASignature()
{
}

AlgorithmParameters* SHA1withDSASignature::engineGetParameters() const
{
	return 0;
}

void SHA1withDSASignature::engineSetParameter(const AlgorithmParameterSpec& spec) throw (InvalidAlgorithmParameterException)
{
	throw InvalidAlgorithmParameterException("not supported for this algorithm");
}

void SHA1withDSASignature::engineInitSign(const PrivateKey& key, SecureRandom* random) throw (InvalidKeyException)
{
	const DSAPrivateKey* dsa = dynamic_cast<const DSAPrivateKey*>(&key);

	if (dsa)
	{
		/* copy key information */
		_params.p = dsa->getParams().getP();
		_params.q = dsa->getParams().getQ();
		_params.g = dsa->getParams().getG();
		_x = dsa->getX();

		/* reset the hash function */
		sha1Reset(&_sp);

		_srng = random;
	}
	else
		throw InvalidKeyException("key must be a DSAPrivateKey");
}

void SHA1withDSASignature::engineInitVerify(const PublicKey& key) throw (InvalidKeyException)
{
	const DSAPublicKey* dsa = dynamic_cast<const DSAPublicKey*>(&key);

	if (dsa)
	{
		/* copy key information */
		_params.p = dsa->getParams().getP();
		_params.q = dsa->getParams().getQ();
		_params.g = dsa->getParams().getG();
		_y = dsa->getY();

		/* reset the hash function */
		sha1Reset(&_sp);

		_srng = 0;
	}
	else
		throw InvalidKeyException("key must be a DSAPrivateKey");
}

void SHA1withDSASignature::engineUpdate(byte b)
{
	sha1Update(&_sp, &b, 1);
}

void SHA1withDSASignature::engineUpdate(const byte* data, size_t offset, size_t len)
{
	sha1Update(&_sp, data+offset, len);
}

void SHA1withDSASignature::rawsign(mpnumber& r, mpnumber& s) throw (SignatureException)
{
	mpnumber hm;
	byte digest[20];

	sha1Digest(&_sp, digest);
	mpnsetbin(&hm, digest, 20);

	if (_srng)
	{
		randomGeneratorContextAdapter rngc(_srng);
		if (dsasign(&_params.p, &_params.q, &_params.g, &rngc, &hm, &_x, &r, &s))
			throw SignatureException("internal error in dsasign function");
	}
	else
	{
		randomGeneratorContext rngc(randomGeneratorDefault());
		if (dsasign(&_params.p, &_params.q, &_params.g, &rngc, &hm, &_x, &r, &s))
			throw SignatureException("internal error in dsasign function");
	}
}

bool SHA1withDSASignature::rawvrfy(const mpnumber& r, const mpnumber& s) throw ()
{
	mpnumber hm;
	byte digest[20];

	sha1Digest(&_sp, digest);
	mpnsetbin(&hm, digest, 20);

	return dsavrfy(&_params.p, &_params.q, &_params.g, &hm, &_y, &r, &s);
}

bytearray* SHA1withDSASignature::engineSign() throw (SignatureException)
{
	mpnumber r, s;

	rawsign(r, s);

	bytearray* signature = new bytearray(asn1_der_length_of_rssig(r, s));

	asn1_der_encode_rssig(signature->data(), r, s);

	return signature;
}

size_t SHA1withDSASignature::engineSign(byte* signature, size_t offset, size_t len) throw (ShortBufferException, SignatureException)
{
	if (!signature)
		throw NullPointerException();

	mpnumber r, s;

	rawsign(r, s);

	if (asn1_der_length_of_rssig(r, s) > (len - offset))
		throw ShortBufferException();

	return asn1_der_encode_rssig(signature+offset, r, s);
}

size_t SHA1withDSASignature::engineSign(bytearray& signature) throw (SignatureException)
{
	mpnumber r, s;

	rawsign(r, s);

	signature.resize(asn1_der_length_of_rssig(r, s));

	return asn1_der_encode_rssig(signature.data(), r, s);
}

bool SHA1withDSASignature::engineVerify(const byte* signature, size_t offset, size_t len) throw (SignatureException)
{
	if (!signature)
		throw NullPointerException();

	mpnumber r, s;

	try
	{
		asn1_der_decode_rssig(signature+offset, len-offset, r, s);
	}
	catch (asn1error ae)
	{
		throw SignatureException("invalid signature");
	}

	return rawvrfy(r, s);
}
