/*
 * dhaes.c
 *
 * DHAES, code
 *
 * This code implements the encryption scheme from the paper:
 *
 * "DHAES: An Encryption Scheme Based on the Diffie-Hellman Problem"
 * Michel Abdalla, Mihir Bellare, Phillip Rogaway
 * September 1998
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited, B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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
 *
 */

#define BEECRYPT_DLL_EXPORT
 
#include "dhaes.h"
#include "dlsvdp-dh.h"
#include "blockmode.h"
#include "blockpad.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

/**
 *
 * Good combinations will be:
 *
 * For 64-bit encryption:
 *	DHAES(Blowfish, MD5/HMAC, MD5)
 *	DHAES(Blowfish, SHA-1/HMAC, MD5)
 *  DHAES(Blowfish, SHA-256/HMAC, MD5)
 *
 * For 128-bit encryption:
 *	DHAES(Blowfish, MD5/HMAC, SHA-256)
 *	DHAES(Blowfish, SHA-1/HMAC, SHA-256)
 *  DHAES(Blowfish, SHA-256/HMAC, SHA-256)
 *
 */

int dhaes_usable(const blockCipher* cipher, const keyedHashFunction* mac, const hashFunction* hash)
{
	int keybits = hash->digestsize << 4;

	/* test if keybits is a multiple of 32 */
	if ((keybits & 31) != 0)
		return 0;

	/* test if keybits length is appropriate for cipher */
	if ((keybits < cipher->keybitsmin) || (keybits > cipher->keybitsmax))
		return 0;

	if (((keybits - cipher->keybitsmin) % cipher->keybitsinc) != 0)
		return 0;

	/* test if keybits length is appropriate for mac */
	if ((keybits < mac->keybitsmin) || (keybits > mac->keybitsmax))
		return 0;

	if (((keybits - mac->keybitsmin) % mac->keybitsinc) != 0)
		return 0;

	return 1;
}

int dhaes_pInit(dhaes_p* p, const dldp_p* param, const blockCipher* cipher, const keyedHashFunction* mac, const hashFunction* hash, const randomGenerator* rng)
{
	if (dhaes_usable(cipher, mac, hash))
	{
		dldp_pInit(&p->param);
		dldp_pCopy(&p->param, param);

		if (blockCipherContextInit(&p->cipher, cipher))
			return -1;

		if (keyedHashFunctionContextInit(&p->mac, mac))
			return -1;

		if (hashFunctionContextInit(&p->hash, hash))
			return -1;

		if (randomGeneratorContextInit(&p->rng, rng))
			return -1;

		return 0;
	}
	return -1;
}

int dhaes_pFree(dhaes_p* p)
{
	dldp_pFree(&p->param);

	if (blockCipherContextFree(&p->cipher))
		return -1;

	if (hashFunctionContextFree(&p->hash))
		return -1;

	if (keyedHashFunctionContextFree(&p->mac))
		return -1;

	if (randomGeneratorContextFree(&p->rng))
		return -1;

	return 0;
}

static int dhaes_pSetup(dhaes_p* p, const mp32number* private, const mp32number* public, const mp32number* message, cipherOperation op)
{
	register int rc;

	mp32number secret;
	mp32number digest;

	/* compute the shared secret, Diffie-Hellman style */
	mp32nzero(&secret);
	dlsvdp_pDHSecret(&p->param, private, public, &secret);

	/* compute the hash of the message (ephemeral public) key and the shared secret */
	mp32nzero(&digest);
	hashFunctionContextReset     (&p->hash);
	hashFunctionContextUpdateMP32(&p->hash, message);
	hashFunctionContextUpdateMP32(&p->hash, &secret);
	hashFunctionContextDigest    (&p->hash, &digest);

	/* we don't need the secret anymore */
	mp32nwipe(&secret);
	mp32nfree(&secret);

	/**
	 * NOTE: blockciphers and keyed hash functions take keys with sizes
	 * specified in bits and key data passed in 32-bit words.
	 *
	 * Both blockcipher and keyed hash function have a min and max key size.
	 *
	 * This function will split the digest of the shared secret in two halves,
	 * and pad with zero bits or truncate if necessary to meet algorithm key
	 * size requirements.
	 */
	
	if ((digest.size & 1) == 0)
	{	/* digest contains an even number of 32 bit words */
		int keysize = digest.size >> 1;
		int keybits = digest.size << 4;

		if ((rc = keyedHashFunctionContextSetup(&p->mac, digest.data, keybits)))
			goto setup_end;

		if ((rc = blockCipherContextSetup(&p->cipher, digest.data+keysize, keybits, op)))
			goto setup_end;

		rc = 0;
	}
	else
		rc = -1;

setup_end:
	mp32nwipe(&digest);
	mp32nfree(&digest);

	return rc;
}

memchunk* dhaes_pEncrypt(dhaes_p* p, const mp32number* publicKey, mp32number* ephemeralPublicKey, mp32number* mac, const memchunk* cleartext)
{
	memchunk* ciphertext = (memchunk*) 0;
	memchunk* paddedtext;

	mp32number ephemeralPrivateKey;

	/* make the ephemeral keypair */
	mp32nzero(&ephemeralPrivateKey);
	dldp_pPair(&p->param, &p->rng, &ephemeralPrivateKey, ephemeralPublicKey);

	/* Setup the key and initialize the mac and the blockcipher */
	if (dhaes_pSetup(p, &ephemeralPrivateKey, publicKey, ephemeralPublicKey, ENCRYPT))
		goto encrypt_end;

	/* add pkcs-5 padding */
	paddedtext = pkcs5Pad(p->cipher.ciph->blocksize, cleartext);

	/* encrypt the memchunk in CBC mode */
	if (blockEncrypt(p->cipher.ciph, p->cipher.param, CBC, paddedtext->size / p->cipher.ciph->blocksize, (uint32*) paddedtext->data, (const uint32*) paddedtext->data))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto encrypt_end;
	}

	/* Compute the mac */
	if (keyedHashFunctionContextUpdateMC(&p->mac, paddedtext))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto encrypt_end;
	}

	if (keyedHashFunctionContextDigest(&p->mac, mac))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto encrypt_end;
	}

	ciphertext = paddedtext;

encrypt_end:
	mp32nwipe(&ephemeralPrivateKey);
	mp32nfree(&ephemeralPrivateKey);

	return ciphertext;
}

memchunk* dhaes_pDecrypt(dhaes_p* p, const mp32number* privateKey, const mp32number* ephemeralPublicKey, const mp32number* mac, const memchunk* ciphertext)
{
	memchunk* cleartext = (memchunk*) 0;
	memchunk* paddedtext;

	/* Setup the key and initialize the mac and the blockcipher */
	if (dhaes_pSetup(p, privateKey, ephemeralPublicKey, ephemeralPublicKey, DECRYPT))
		goto decrypt_end;

	/* Verify the mac */
	if (keyedHashFunctionContextUpdateMC(&p->mac, ciphertext))
		goto decrypt_end;

	if (keyedHashFunctionContextDigestMatch(&p->mac, mac) == 0)
		goto decrypt_end;

	/* decrypt the memchunk with CBC mode */
	paddedtext = (memchunk*) calloc(1, sizeof(memchunk));

	if (paddedtext == (memchunk*) 0)
		goto decrypt_end;

	paddedtext->size = ciphertext->size;
	paddedtext->data = (byte*) malloc(ciphertext->size);

	if (paddedtext->data == (byte*) 0)
	{
		free(paddedtext);
		goto decrypt_end;
	}

	if (blockDecrypt(p->cipher.ciph, p->cipher.param, CBC, paddedtext->size / p->cipher.ciph->blocksize, (uint32*) paddedtext->data, (const uint32*) ciphertext->data))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto decrypt_end;
	}

	/* remove pkcs-5 padding */
	if (pkcs5UnpadInline(p->cipher.ciph->blocksize, paddedtext))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto decrypt_end;
	}

	cleartext = paddedtext;

decrypt_end:

	return cleartext;
}
