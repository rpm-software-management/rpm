/*
 * beecrypt.c
 *
 * BeeCrypt library hooks & stubs, code
 *
 * Copyright (c) 1999-2000 Virtual Unlimited B.V.
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
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif

#include "entropy.h"
#include "fips180.h"
#include "fips186.h"
#include "mtprng.h"
#include "sha1hmac.h"

#include "blowfish.h"
#include "blockmode.h"

static entropySource entropySourceList[] =
{
#if WIN32
	{ "wavein", entropy_wavein },
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
	char* tmp = getenv("BEECRYPT_ENTROPY");
	if (tmp)
	{
		return entropySourceFind(tmp);
	}
	else if (ENTROPYSOURCES)
	{
		return entropySourceList+0;
	}
	return (const entropySource*) 0;
}

static const randomGenerator* randomGeneratorList[] =
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
	char* tmp = getenv("BEECRYPT_RANDOM");

	if (tmp)
		return randomGeneratorFind(tmp);
	else
		return &fips186prng;
}

void randomGeneratorContextInit(randomGeneratorContext* ctxt, const randomGenerator* rng)
{
	ctxt->rng = rng;
	ctxt->param = malloc(rng->paramsize);
}

void randomGeneratorContextFree(randomGeneratorContext* ctxt)
{
	free(ctxt->param);
}

static const hashFunction* hashFunctionList[] =
{
	&sha1
};

#define HASHFUNCTIONS (sizeof(hashFunctionList) / sizeof(hashFunction*))

int hashFunctionCount()
{
	return HASHFUNCTIONS;
}

const hashFunction* hashFunctionDefault()
{
	char* tmp = getenv("BEECRYPT_HASH");
	if (tmp)
		return hashFunctionFind(tmp);
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

void hashFunctionContextInit(hashFunctionContext* ctxt, const hashFunction* hash)
{
	ctxt->hash = hash;
	ctxt->param = malloc(hash->paramsize);
}

void hashFunctionContextFree(hashFunctionContext* ctxt)
{
	free(ctxt->param);
}


static const keyedHashFunction* keyedHashFunctionList[] =
{
	&sha1hmac
};

#define KEYEDHASHFUNCTIONS 	(sizeof(keyedHashFunctionList) / sizeof(keyedHashFunction*))

int keyedHashFunctionCount()
{
	return KEYEDHASHFUNCTIONS;
}

const keyedHashFunction* keyedHashFunctionDefault()
{
	char* tmp = getenv("BEECRYPT_KEYEDHASH");
	if (tmp)
		return keyedHashFunctionFind(tmp);
	else
		return (const keyedHashFunction*) 0;
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

void keyedHashFunctionContextInit(keyedHashFunctionContext* ctxt, const keyedHashFunction* hash)
{
	ctxt->hash = hash;
	ctxt->param = malloc(hash->paramsize);
}

void keyedHashFunctionContextFree(keyedHashFunctionContext* ctxt)
{
	free(ctxt->param);
}


static const blockCipher* blockCipherList[] =
{
	&blowfish,
};

#define BLOCKCIPHERS (sizeof(blockCipherList) / sizeof(blockCipher*))

int blockCipherCount()
{
	return BLOCKCIPHERS;
}

const blockCipher* blockCipherDefault()
{
	char* tmp = getenv("BEECRYPT_CIPHER");

	if (tmp)
		return blockCipherFind(tmp);
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

void blockCipherContextInit(blockCipherContext* ctxt, const blockCipher* ciph)
{
	ctxt->ciph = ciph;
	ctxt->param = malloc(ciph->paramsize);
}

void blockCipherContextFree(blockCipherContext* ctxt)
{
	free(ctxt->param);
}
