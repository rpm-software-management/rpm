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
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

/**
 * Good combinations will be:
 *
 * For 64-bit encryption:
 *	DHAES(MD5, Blowfish, HMAC-MD5) <- best candidate
 *	DHAES(MD5, Blowfish, HMAC-SHA-1)
 *  DHAES(MD5, Blowfish, HMAC-SHA-256)
 *
 * For 96-bit encryption with 64-bit mac:
 *  DHAES(SHA-1, Blowfish, HMAC-MD5, 96)
 *  DHAES(SHA-1, Blowfish, HMAC-SHA-1, 96) <- best candidate
 *  DHAES(SHA-1, Blowfish, HMAC-SHA-256, 96) <- best candidate
 *
 * For 128-bit encryption:
 *	DHAES(SHA-256, Blowfish, HMAC-MD5)
 *	DHAES(SHA-256, Blowfish, HMAC-SHA-1)
 *  DHAES(SHA-256, Blowfish, HMAC-SHA-256)
 */

int dhaes_pUsable(const dhaes_pParameters* params)
{
	int keybits = (params->hash->digestsize << 3); /* digestsize in bytes times 8 bits */
	int cipherkeybits = params->cipherkeybits;
	int mackeybits = params->mackeybits;

	/* test if keybits is a multiple of 32 */
	if ((keybits & 31) != 0)
		return 0;

	/* test if cipherkeybits + mackeybits < keybits */
	if ((cipherkeybits + mackeybits) > keybits)
		return 0;

	if (mackeybits == 0)
	{
		if (cipherkeybits == 0)
			cipherkeybits = mackeybits = (keybits >> 1);
		else
			mackeybits = keybits - cipherkeybits;
	}

	/* test if keybits length is appropriate for cipher */
	if ((cipherkeybits < params->cipher->keybitsmin) ||
			(cipherkeybits > params->cipher->keybitsmax))
		return 0;

	if (((cipherkeybits - params->cipher->keybitsmin) % params->cipher->keybitsinc) != 0)
		return 0;

	/* test if keybits length is appropriate for mac */
	if ((mackeybits < params->mac->keybitsmin) ||
			(params->mackeybits > params->mac->keybitsmax))
		return 0;

	if (((mackeybits - params->mac->keybitsmin) % params->mac->keybitsinc) != 0)
		return 0;

	return 1;
}

int dhaes_pContextInit(dhaes_pContext* ctxt, const dhaes_pParameters* params)
{
	if (ctxt == (dhaes_pContext*) 0)
		return -1;

	if (params == (dhaes_pParameters*) 0)
		return -1;

	if (params->param == (dldp_p*) 0)
		return -1;

	if (params->hash == (hashFunction*) 0)
		return -1;

	if (params->cipher == (blockCipher*) 0)
		return -1;

	if (params->mac == (keyedHashFunction*) 0)
		return -1;

	if (!dhaes_pUsable(params))
		return -1;

	(void) dldp_pInit(&ctxt->param);
	(void) dldp_pCopy(&ctxt->param, params->param);

	mp32nzero(&ctxt->pub);
	mp32nzero(&ctxt->pri);

	if (hashFunctionContextInit(&ctxt->hash, params->hash))
		return -1;

	if (blockCipherContextInit(&ctxt->cipher, params->cipher))
		return -1;

	if (keyedHashFunctionContextInit(&ctxt->mac, params->mac))
		return -1;

	ctxt->cipherkeybits = params->cipherkeybits;
	ctxt->mackeybits = params->mackeybits;

	return 0;
}

int dhaes_pContextInitDecrypt(dhaes_pContext* ctxt, const dhaes_pParameters* params, const mp32number* pri)
{
	if (dhaes_pContextInit(ctxt, params))
		return -1;

	mp32ncopy(&ctxt->pri, pri);

	return 0;
}

int dhaes_pContextInitEncrypt(dhaes_pContext* ctxt, const dhaes_pParameters* params, const mp32number* pub)
{
	if (dhaes_pContextInit(ctxt, params))
		return -1;

	mp32ncopy(&ctxt->pub, pub);

	return 0;
}

int dhaes_pContextFree(dhaes_pContext* ctxt)
{
	(void) dldp_pFree(&ctxt->param);

	mp32nfree(&ctxt->pub);
	mp32nfree(&ctxt->pri);

	if (hashFunctionContextFree(&ctxt->hash))
		return -1;

	if (blockCipherContextFree(&ctxt->cipher))
		return -1;

	if (keyedHashFunctionContextFree(&ctxt->mac))
		return -1;

	return 0;
}

static int dhaes_pContextSetup(dhaes_pContext* ctxt, const mp32number* private, const mp32number* public, const mp32number* message, cipherOperation op)
{
	register int rc;

	mp32number secret;
	mp32number digest;

	/* compute the shared secret, Diffie-Hellman style */
	mp32nzero(&secret);
	if (dlsvdp_pDHSecret(&ctxt->param, private, public, &secret))
		return -1;

	/* compute the hash of the message (ephemeral public) key and the shared secret */
	mp32nzero(&digest);
	(void) hashFunctionContextReset     (&ctxt->hash);
	(void) hashFunctionContextUpdateMP32(&ctxt->hash, message);
	(void) hashFunctionContextUpdateMP32(&ctxt->hash, &secret);
	(void) hashFunctionContextDigest    (&ctxt->hash, &digest);

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

	if (digest.size > 0)
	{
		uint32* mackey = digest.data;
		uint32* cipherkey = digest.data + ((ctxt->mackeybits + 31) >> 5);

		if ((rc = keyedHashFunctionContextSetup(&ctxt->mac, mackey, ctxt->mackeybits)))
			goto setup_end;

		if ((rc = blockCipherContextSetup(&ctxt->cipher, cipherkey, ctxt->cipherkeybits, op)))
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

memchunk* dhaes_pContextEncrypt(dhaes_pContext* ctxt, mp32number* ephemeralPublicKey, mp32number* mac, const memchunk* cleartext, randomGeneratorContext* rng)
{
	memchunk* ciphertext = (memchunk*) 0;
	memchunk* paddedtext;

	mp32number ephemeralPrivateKey;

	/* make the ephemeral keypair */
	mp32nzero(&ephemeralPrivateKey);
	(void) dldp_pPair(&ctxt->param, rng, &ephemeralPrivateKey, ephemeralPublicKey);

	/* Setup the key and initialize the mac and the blockcipher */
	if (dhaes_pContextSetup(ctxt, &ephemeralPrivateKey, &ctxt->pub, ephemeralPublicKey, ENCRYPT))
		goto encrypt_end;

	/* add pkcs-5 padding */
	paddedtext = pkcs5PadCopy(ctxt->cipher.algo->blocksize, cleartext);
	if (paddedtext == (memchunk*) 0)
		goto encrypt_end;

	/* encrypt the memchunk in CBC mode */
	if (blockEncrypt(ctxt->cipher.algo, ctxt->cipher.param, CBC, paddedtext->size / ctxt->cipher.algo->blocksize, (uint32*) paddedtext->data, (const uint32*) paddedtext->data))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto encrypt_end;
	}

	/* Compute the mac */
	if (keyedHashFunctionContextUpdateMC(&ctxt->mac, paddedtext))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto encrypt_end;
	}

	if (keyedHashFunctionContextDigest(&ctxt->mac, mac))
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

memchunk* dhaes_pContextDecrypt(dhaes_pContext* ctxt, const mp32number* ephemeralPublicKey, const mp32number* mac, const memchunk* ciphertext)
{
	memchunk* cleartext = (memchunk*) 0;
	memchunk* paddedtext;

	/* Setup the key and initialize the mac and the blockcipher */
	if (dhaes_pContextSetup(ctxt, &ctxt->pri, ephemeralPublicKey, ephemeralPublicKey, DECRYPT))
		goto decrypt_end;

	/* Verify the mac */
	if (keyedHashFunctionContextUpdateMC(&ctxt->mac, ciphertext))
		goto decrypt_end;

	if (keyedHashFunctionContextDigestMatch(&ctxt->mac, mac) == 0)
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

	if (blockDecrypt(ctxt->cipher.algo, ctxt->cipher.param, CBC, paddedtext->size / ctxt->cipher.algo->blocksize, (uint32*) paddedtext->data, (const uint32*) ciphertext->data))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto decrypt_end;
	}

	/* remove pkcs-5 padding */
	cleartext = pkcs5Unpad(ctxt->cipher.algo->blocksize, paddedtext);

	if (cleartext == (memchunk*) 0)
	{
		free(paddedtext->data);
		free(paddedtext);
	}

decrypt_end:

	return cleartext;
}
