/*
 * beecrypt.c
 *
 * BeeCrypt library hooks & stubs, code
 *
 * Copyright (c) 1999, 2000, 2001 Virtual Unlimited B.V.
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

#include "beecrypt.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if WIN32
# include <windows.h>
#endif

#include "endianness.h"
#include "entropy.h"
#include "fips180.h"
#include "fips186.h"
#include "hmacmd5.h"
#include "hmacsha1.h"
#include "hmacsha256.h"
#include "md5.h"
#include "mp32.h"
#include "mtprng.h"
#include "sha256.h"

#include "blowfish.h"
#include "blockmode.h"

/*@observer@*/ static entropySource entropySourceList[] =
{
#if WIN32
	{ "wavein", entropy_wavein },
	{ "console", entropy_console },
	{ "wincrypt", entropy_wincrypt },
#else
# if HAVE_DEV_AUDIO
	{ "audio", entropy_dev_audio },
# endif
# if HAVE_DEV_DSP
	{ "dsp", entropy_dev_dsp },
# endif
# if HAVE_DEV_RANDOM
	{ "random", entropy_dev_random },
# endif
# if HAVE_DEV_URANDOM
	{ "urandom", entropy_dev_urandom },
# endif
# if HAVE_DEV_TTY
	{ "tty", entropy_dev_tty },
# endif
#endif
};

#define ENTROPYSOURCES (sizeof(entropySourceList) / sizeof(entropySource))

int entropySourceCount()
{
	return ENTROPYSOURCES;
}

const entropySource* entropySourceGet(int index)
{
	if ((index < 0) || (index >= ENTROPYSOURCES))
		return (const entropySource*) 0;

	return entropySourceList+index;
}

const entropySource* entropySourceFind(const char* name)
{
	register int index;

	for (index = 0; index < ENTROPYSOURCES; index++)
	{
		if (strcmp(name, entropySourceList[index].name) == 0)
			return entropySourceList+index;
	}
	return (const entropySource*) 0;
}

const entropySource* entropySourceDefault()
{
	const char* selection = getenv("BEECRYPT_ENTROPY");

	if (selection)
	{
		return entropySourceFind(selection);
	}
	else if (ENTROPYSOURCES)
	{
		return entropySourceList+0;
	}
	return (const entropySource*) 0;
}

int entropyGatherNext(uint32* data, int size)
{
	const char* selection = getenv("BEECRYPT_ENTROPY");

	if (selection)
	{
		const entropySource* ptr = entropySourceFind(selection);

		if (ptr)
			return ptr->next(data, size);
	}
	else
	{
		register int index;

		for (index = 0; index < ENTROPYSOURCES; index++)
		{
			if (entropySourceList[index].next(data, size) == 0)
				return 0;
		}
	}
	return -1;
}

/*@observer@*/ static const randomGenerator* randomGeneratorList[] =
{
	&fips186prng,
	&mtprng
};

#define RANDOMGENERATORS	(sizeof(randomGeneratorList) / sizeof(randomGenerator*))

int randomGeneratorCount()
{
	return RANDOMGENERATORS;
}

const randomGenerator* randomGeneratorGet(int index)
{
	if ((index < 0) || (index >= RANDOMGENERATORS))
		return (const randomGenerator*) 0;

	return randomGeneratorList[index];
}

const randomGenerator* randomGeneratorFind(const char* name)
{
	register int index;

	for (index = 0; index < RANDOMGENERATORS; index++)
	{
		if (strcmp(name, randomGeneratorList[index]->name) == 0)
			return randomGeneratorList[index];
	}
	return (const randomGenerator*) 0;
}

const randomGenerator* randomGeneratorDefault()
{
	char* selection = getenv("BEECRYPT_RANDOM");

	if (selection)
		return randomGeneratorFind(selection);
	else
		return &fips186prng;
}

int randomGeneratorContextInit(randomGeneratorContext* ctxt, const randomGenerator* rng)
{
	if (ctxt == (randomGeneratorContext*) 0)
		return -1;

	if (rng == (randomGenerator*) 0)
		return -1;

	ctxt->rng = rng;
	ctxt->param = (randomGeneratorParam*) calloc(rng->paramsize, 1);

	if (ctxt->param == (randomGeneratorParam*) 0)
		return -1;

	return ctxt->rng->setup(ctxt->param);
}

int randomGeneratorContextFree(randomGeneratorContext* ctxt)
{
	register int rc;

	if (ctxt == (randomGeneratorContext*) 0)
		return -1;

	if (ctxt->rng == (randomGenerator*) 0)
		return -1;

	if (ctxt->param == (randomGeneratorParam*) 0)
		return -1;

	rc = ctxt->rng->cleanup(ctxt->param);

	free(ctxt->param);

	ctxt->param = (randomGeneratorParam*) 0;

	return rc;
}

/*@observer@*/ static const hashFunction* hashFunctionList[] =
{
	&md5,
	&sha1,
	&sha256
};

#define HASHFUNCTIONS (sizeof(hashFunctionList) / sizeof(hashFunction*))

int hashFunctionCount()
{
	return HASHFUNCTIONS;
}

const hashFunction* hashFunctionDefault()
{
	char* selection = getenv("BEECRYPT_HASH");

	if (selection)
		return hashFunctionFind(selection);
	else
		return &sha1;
}

const hashFunction* hashFunctionGet(int index)
{
	if ((index < 0) || (index >= HASHFUNCTIONS))
		return (const hashFunction*) 0;

	return hashFunctionList[index];
}

const hashFunction* hashFunctionFind(const char* name)
{
	register int index;

	for (index = 0; index < HASHFUNCTIONS; index++)
	{
		if (strcmp(name, hashFunctionList[index]->name) == 0)
			return hashFunctionList[index];
	}
	return (const hashFunction*) 0;
}

int hashFunctionContextInit(hashFunctionContext* ctxt, const hashFunction* hash)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (hash == (hashFunction*) 0)
		return -1;

	ctxt->algo = hash;
	ctxt->param = (hashFunctionParam*) calloc(hash->paramsize, 1);

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	return ctxt->algo->reset(ctxt->param);
}

int hashFunctionContextFree(hashFunctionContext* ctxt)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	free(ctxt->param);

	ctxt->param = (hashFunctionParam*) 0;

	return 0;
}

int hashFunctionContextReset(hashFunctionContext* ctxt)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (hashFunction*) 0)
		return -1;

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	return ctxt->algo->reset(ctxt->param);
}

int hashFunctionContextUpdate(hashFunctionContext* ctxt, const byte* data, int size)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (hashFunction*) 0)
		return -1;

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	if (data == (const byte*) 0)
		return -1;

	return ctxt->algo->update(ctxt->param, data, size);
}

int hashFunctionContextUpdateMC(hashFunctionContext* ctxt, const memchunk* m)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (hashFunction*) 0)
		return -1;

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	if (m == (memchunk*) 0)
		return -1;

	return ctxt->algo->update(ctxt->param, m->data, m->size);
}

int hashFunctionContextUpdateMP32(hashFunctionContext* ctxt, const mp32number* n)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (hashFunction*) 0)
		return -1;

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	if (n != (mp32number*) 0)
	{
		register int rc = -1;
		register byte* temp = (byte*) malloc((n->size << 2) + 1);

		if (mp32msbset(n->size, n->data))
		{
			temp[0] = 0;
			encodeInts((javaint*) n->data, temp+1, n->size);
			rc = ctxt->algo->update(ctxt->param, temp, (n->size << 2) + 1);
		}
		else
		{
			encodeInts((javaint*) n->data, temp, n->size);
			rc = ctxt->algo->update(ctxt->param, temp, n->size << 2);
		}
		free(temp);

		return rc;
	}
	return -1;
}

int hashFunctionContextDigest(hashFunctionContext* ctxt, mp32number* dig)
{
	if (ctxt == (hashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (hashFunction*) 0)
		return -1;

	if (ctxt->param == (hashFunctionParam*) 0)
		return -1;

	if (dig != (mp32number*) 0)
	{
		mp32nsize(dig, (ctxt->algo->digestsize + 3) >> 2);

		return ctxt->algo->digest(ctxt->param, dig->data);
	}
	return -1;
}

int hashFunctionContextDigestMatch(hashFunctionContext* ctxt, const mp32number* match)
{
	register int rc = 0;

	mp32number dig;

	mp32nzero(&dig);

	if (hashFunctionContextDigest(ctxt, &dig) == 0)
		if (dig.size == match->size)
			rc = mp32eq(dig.size, dig.data, match->data);

	mp32nfree(&dig);

	return rc;
}

/*@observer@*/ static const keyedHashFunction* keyedHashFunctionList[] =
{
	&hmacmd5,
	&hmacsha1,
	&hmacsha256
};

#define KEYEDHASHFUNCTIONS 	(sizeof(keyedHashFunctionList) / sizeof(keyedHashFunction*))

int keyedHashFunctionCount()
{
	return KEYEDHASHFUNCTIONS;
}

const keyedHashFunction* keyedHashFunctionDefault()
{
	char* selection = getenv("BEECRYPT_KEYEDHASH");

	if (selection)
		return keyedHashFunctionFind(selection);
	else
		return &hmacsha1;
}

const keyedHashFunction* keyedHashFunctionGet(int index)
{
	if ((index < 0) || (index >= KEYEDHASHFUNCTIONS))
		return (const keyedHashFunction*) 0;

	return keyedHashFunctionList[index];
}

const keyedHashFunction* keyedHashFunctionFind(const char* name)
{
	register int index;

	for (index = 0; index < KEYEDHASHFUNCTIONS; index++)
	{
		if (strcmp(name, keyedHashFunctionList[index]->name) == 0)
			return keyedHashFunctionList[index];
	}
	return (const keyedHashFunction*) 0;
}

int keyedHashFunctionContextInit(keyedHashFunctionContext* ctxt, const keyedHashFunction* mac)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (mac == (keyedHashFunction*) 0)
		return -1;

	ctxt->algo = mac;
	ctxt->param = (keyedHashFunctionParam*) calloc(mac->paramsize, 1);

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	return ctxt->algo->reset(ctxt->param);
}

int keyedHashFunctionContextFree(keyedHashFunctionContext* ctxt)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	free(ctxt->param);

	ctxt->param = (keyedHashFunctionParam*) 0;

	return 0;
}

int keyedHashFunctionContextSetup(keyedHashFunctionContext* ctxt, const uint32* key, int keybits)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	if (key == (uint32*) 0)
		return -1;

	return ctxt->algo->setup(ctxt->param, key, keybits);
}

int keyedHashFunctionContextReset(keyedHashFunctionContext* ctxt)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	return ctxt->algo->reset(ctxt->param);
}

int keyedHashFunctionContextUpdate(keyedHashFunctionContext* ctxt, const byte* data, int size)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	if (data == (byte*) 0)
		return -1;

	return ctxt->algo->update(ctxt->param, data, size);
}

int keyedHashFunctionContextUpdateMC(keyedHashFunctionContext* ctxt, const memchunk* m)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	if (m == (memchunk*) 0)
		return -1;

	return ctxt->algo->update(ctxt->param, m->data, m->size);
}

int keyedHashFunctionContextUpdateMP32(keyedHashFunctionContext* ctxt, const mp32number* n)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	if (n != (mp32number*) 0)
	{
		register int rc;
		register byte* temp = (byte*) malloc((n->size << 2) + 1);

		if (mp32msbset(n->size, n->data))
		{
			temp[0] = 0;
			encodeInts((javaint*) n->data, temp+1, n->size);
			rc = ctxt->algo->update(ctxt->param, temp, (n->size << 2) + 1);
		}
		else
		{
			encodeInts((javaint*) n->data, temp, n->size);
			rc = ctxt->algo->update(ctxt->param, temp, n->size << 2);
		}
		free(temp);

		return rc;
	}
	return -1;
}

int keyedHashFunctionContextDigest(keyedHashFunctionContext* ctxt, mp32number* dig)
{
	if (ctxt == (keyedHashFunctionContext*) 0)
		return -1;

	if (ctxt->algo == (keyedHashFunction*) 0)
		return -1;

	if (ctxt->param == (keyedHashFunctionParam*) 0)
		return -1;

	if (dig != (mp32number*) 0)
	{
		mp32nsize(dig, (ctxt->algo->digestsize + 3) >> 2);

		return ctxt->algo->digest(ctxt->param, dig->data);
	}
	return -1;
}

int keyedHashFunctionContextDigestMatch(keyedHashFunctionContext* ctxt, const mp32number* match)
{
	register int rc = 0;

	mp32number dig;

	mp32nzero(&dig);

	if (keyedHashFunctionContextDigest(ctxt, &dig) == 0)
		if (dig.size == match->size)
			
			rc = mp32eq(dig.size, dig.data, match->data);

	mp32nfree(&dig);

	return rc;
}


/*@observer@*/ static const blockCipher* blockCipherList[] =
{
	&blowfish
};

#define BLOCKCIPHERS (sizeof(blockCipherList) / sizeof(blockCipher*))

int blockCipherCount()
{
	return BLOCKCIPHERS;
}

const blockCipher* blockCipherDefault()
{
	char* selection = getenv("BEECRYPT_CIPHER");

	if (selection)
		return blockCipherFind(selection);
	else
		return &blowfish;
}

const blockCipher* blockCipherGet(int index)
{
	if ((index < 0) || (index >= BLOCKCIPHERS))
		return (const blockCipher*) 0;

	return blockCipherList[index];
}

const blockCipher* blockCipherFind(const char* name)
{
	register int index;

	for (index = 0; index < BLOCKCIPHERS; index++)
	{
		if (strcmp(name, blockCipherList[index]->name) == 0)
			return blockCipherList[index];
	}

	return (const blockCipher*) 0;
}

int blockCipherContextInit(blockCipherContext* ctxt, const blockCipher* ciph)
{
	if (ctxt == (blockCipherContext*) 0)
		return -1;

	if (ciph == (blockCipher*) 0)
		return -1;

	ctxt->algo = ciph;
	ctxt->param = (blockCipherParam*) calloc(ciph->paramsize, 1);

	if (ctxt->param == (blockCipherParam*) 0)
		return -1;

	return 0;
}

int blockCipherContextSetup(blockCipherContext* ctxt, const uint32* key, int keybits, cipherOperation op)
{
	if (ctxt == (blockCipherContext*) 0)
		return -1;

	if (ctxt->algo == (blockCipher*) 0)
		return -1;

	if (ctxt->param == (blockCipherParam*) 0)
		return -1;

	if (key == (uint32*) 0)
		return -1;

	return ctxt->algo->setup(ctxt->param, key, keybits, op);
}

int blockCipherContextSetIV(blockCipherContext* ctxt, const uint32* iv)
{
	if (ctxt == (blockCipherContext*) 0)
		return -1;

	if (ctxt->algo == (blockCipher*) 0)
		return -1;

	if (ctxt->param == (blockCipherParam*) 0)
		return -1;

	/* null is an allowed value for iv, so don't test it */

	return ctxt->algo->setiv(ctxt->param, iv);
}

int blockCipherContextFree(blockCipherContext* ctxt)
{
	if (ctxt == (blockCipherContext*) 0)
		return -1;

	if (ctxt->param == (blockCipherParam*) 0)
		return -1;

	free(ctxt->param);

	ctxt->param = (blockCipherParam*) 0;

	return 0;
}

#if WIN32
__declspec(dllexport)
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD wDataSeg, LPVOID lpReserved)
{
	switch (wDataSeg)
	{
   	case DLL_PROCESS_ATTACH:
   		entropy_provider_setup(hInst);
   		break;
	case DLL_PROCESS_DETACH:
		entropy_provider_cleanup();
		break;
   	}
   	return TRUE;
}
#endif
