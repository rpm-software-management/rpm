/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited, B.V.
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

/*!\file dhaes.c
 * \brief DHAES encryption scheme.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m DL_dh_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/dhaes.h"
#include "beecrypt/dlsvdp-dh.h"
#include "beecrypt/blockmode.h"
#include "beecrypt/blockpad.h"

/*
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
	size_t keybits = (params->hash->digestsize << 3); /* digestsize in bytes times 8 bits */
	size_t cipherkeybits = params->cipherkeybits;
	size_t mackeybits = params->mackeybits;

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

	dldp_pInit(&ctxt->param);
	dldp_pCopy(&ctxt->param, params->param);

	mpnzero(&ctxt->pub);
	mpnzero(&ctxt->pri);

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

int dhaes_pContextInitDecrypt(dhaes_pContext* ctxt, const dhaes_pParameters* params, const mpnumber* pri)
{
	if (dhaes_pContextInit(ctxt, params))
		return -1;

	mpncopy(&ctxt->pri, pri);

	return 0;
}

int dhaes_pContextInitEncrypt(dhaes_pContext* ctxt, const dhaes_pParameters* params, const mpnumber* pub)
{
	if (dhaes_pContextInit(ctxt, params))
		return -1;

	mpncopy(&ctxt->pub, pub);

	return 0;
}

int dhaes_pContextFree(dhaes_pContext* ctxt)
{
	dldp_pFree(&ctxt->param);

	mpnfree(&ctxt->pub);
	mpnfree(&ctxt->pri);

	if (hashFunctionContextFree(&ctxt->hash))
		return -1;

	if (blockCipherContextFree(&ctxt->cipher))
		return -1;

	if (keyedHashFunctionContextFree(&ctxt->mac))
		return -1;

	return 0;
}

static int dhaes_pContextSetup(dhaes_pContext* ctxt, const mpnumber* private, const mpnumber* public, const mpnumber* message, cipherOperation op)
{
	register int rc;

	mpnumber secret;

	byte* digest = (byte*) malloc(ctxt->hash.algo->digestsize);

	if (digest == (byte*) 0)
		return -1;

	/* compute the shared secret, Diffie-Hellman style */
	mpnzero(&secret);
	if (dlsvdp_pDHSecret(&ctxt->param, private, public, &secret))
	{
		mpnfree(&secret);
		free(digest);
		return -1;
	}

	/* compute the hash of the message (ephemeral public) key and the shared secret */

	hashFunctionContextReset   (&ctxt->hash);
	hashFunctionContextUpdateMP(&ctxt->hash, message);
	hashFunctionContextUpdateMP(&ctxt->hash, &secret);
	hashFunctionContextDigest  (&ctxt->hash, digest);

	/* we don't need the secret anymore */
	mpnwipe(&secret);
	mpnfree(&secret);

	/*
	 * NOTE: blockciphers and keyed hash functions take keys with sizes
	 * specified in bits and key data passed in bytes.
	 *
	 * Both blockcipher and keyed hash function have a min and max key size.
	 *
	 * This function will split the digest of the shared secret in two halves,
	 * and pad with zero bits or truncate if necessary to meet algorithm key
	 * size requirements.
	 */

	if (ctxt->hash.algo->digestsize > 0)
	{
		byte* mackey = digest;
		byte* cipherkey = digest + ((ctxt->mackeybits + 7) >> 3);

		if ((rc = keyedHashFunctionContextSetup(&ctxt->mac, mackey, ctxt->mackeybits)))
			goto setup_end;

		if ((rc = blockCipherContextSetup(&ctxt->cipher, cipherkey, ctxt->cipherkeybits, op)))
			goto setup_end;

		rc = 0;
	}
	else
		rc = -1;

setup_end:
	/* wipe digest for good measure */
	memset(digest, 0, ctxt->hash.algo->digestsize); 
	free(digest);

	return rc;
}

memchunk* dhaes_pContextEncrypt(dhaes_pContext* ctxt, mpnumber* ephemeralPublicKey, mpnumber* mac, const memchunk* cleartext, randomGeneratorContext* rng)
{
	memchunk* ciphertext = (memchunk*) 0;
	memchunk* paddedtext;

	mpnumber ephemeralPrivateKey;

	/* make the ephemeral keypair */
	mpnzero(&ephemeralPrivateKey);
	dldp_pPair(&ctxt->param, rng, &ephemeralPrivateKey, ephemeralPublicKey);

	/* Setup the key and initialize the mac and the blockcipher */
	if (dhaes_pContextSetup(ctxt, &ephemeralPrivateKey, &ctxt->pub, ephemeralPublicKey, ENCRYPT))
		goto encrypt_end;

	/* add pkcs-5 padding */
	paddedtext = pkcs5PadCopy(ctxt->cipher.algo->blocksize, cleartext);

	/* encrypt the memchunk in CBC mode */
	if (blockEncryptCBC(ctxt->cipher.algo, ctxt->cipher.param, (uint32_t*) paddedtext->data, (const uint32_t*) paddedtext->data, paddedtext->size / ctxt->cipher.algo->blocksize))
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

	if (keyedHashFunctionContextDigestMP(&ctxt->mac, mac))
	{
		free(paddedtext->data);
		free(paddedtext);
		goto encrypt_end;
	}

	ciphertext = paddedtext;

encrypt_end:
	mpnwipe(&ephemeralPrivateKey);
	mpnfree(&ephemeralPrivateKey);

	return ciphertext;
}

memchunk* dhaes_pContextDecrypt(dhaes_pContext* ctxt, const mpnumber* ephemeralPublicKey, const mpnumber* mac, const memchunk* ciphertext)
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

	if (blockDecryptCBC(ctxt->cipher.algo, ctxt->cipher.param, (uint32_t*) paddedtext->data, (const uint32_t*) ciphertext->data, paddedtext->size / ctxt->cipher.algo->blocksize))
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
