#define BEECRYPT_DLL_EXPORT

#include "beecrypt/pkcs12.h"

int pkcs12_derive_key(const hashFunction* h, byte id, const byte* pdata, size_t psize, const byte* sdata, size_t ssize, size_t iterationcount, byte* ndata, size_t nsize)
{
	int rc = -1;
	size_t i, remain;
	hashFunctionContext ctxt;
	byte *digest;

	digest = (byte*) malloc(h->digestsize);
	if (!digest)
		goto cleanup;

	if (hashFunctionContextInit(&ctxt, h))
		goto cleanup;

	/* we start by hashing the diversifier; don't allocate a buffer for this */
	for (i = 0; i < h->blocksize; i++)
		hashFunctionContextUpdate(&ctxt, &id, 1);

	/* next we hash the salt data, concatenating until we have a whole number of blocks */
	if (ssize)
	{
		remain = ((ssize / h->blocksize) + (ssize % h->blocksize)) * h->blocksize;
		while (remain > 0)
		{
			size_t tmp = remain > ssize ? ssize : remain;

			hashFunctionContextUpdate(&ctxt, sdata, tmp);

			remain -= tmp;
		}
	}

	/* next we hash the password data, concatenating until we have a whole number of blocks */
	if (psize)
	{
		remain = ((psize / h->blocksize) + (psize % h->blocksize)) * h->blocksize;
		while (remain > 0)
		{
			size_t tmp = remain > psize ? psize : remain;

			hashFunctionContextUpdate(&ctxt, pdata, tmp);

			remain -= tmp;
		}
	}

	/* now we iterate through the following loop */
	while (iterationcount-- > 0)
	{
		hashFunctionContextDigest(&ctxt, digest);
		hashFunctionContextUpdate(&ctxt, digest, h->digestsize);
	}

	/* do the final digest */
	hashFunctionContextDigest(&ctxt, digest);

	/* fill key */
	while (nsize > 0)
	{
		size_t tmp = nsize > h->digestsize ? h->digestsize : nsize;

		memcpy(ndata, digest, tmp);
		ndata += tmp;
		nsize -= tmp;
	}

	if (hashFunctionContextFree(&ctxt))
		goto cleanup;

	rc = 0;

cleanup:
	if (digest)
		free(digest);

	return rc;
}
