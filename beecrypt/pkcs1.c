#define BEECRYPT_DLL_EXPORT

#include "beecrypt/pkcs1.h"

const byte EMSA_MD5_DIGESTINFO[18] = {
	0x30,0x20,0x30,0x0c,0x06,0x08,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x02,0x05,0x05,0x00,
	0x04,0x10
};

const byte EMSA_SHA1_DIGESTINFO[15] = {
	0x30,0x21,0x30,0x09,0x06,0x05,0x2b,0x0e,0x03,0x02,0x1a,0x05,0x00,0x04,0x14
};

const byte EMSA_SHA256_DIGESTINFO[19] = {
	0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,
	0x00,0x04,0x20
};

int pkcs1_emsa_encode_digest(hashFunctionContext* ctxt, byte* emdata, size_t emlen)
{
	int rc = -1;
	const byte* tinfo;
	size_t tlen, digestsize = ctxt->algo->digestsize;

	if (strcmp(ctxt->algo->name, "MD5") == 0)
	{
		/* tlen is 18 bytes for EMSA_MD5_DIGESTINFO plus digestsize */
		tinfo = EMSA_MD5_DIGESTINFO;
		tlen = 18;
	}
	else if (strcmp(ctxt->algo->name, "SHA-1") == 0)
	{
		/* tlen is 15 bytes for EMSA_SHA1_DIGESTINFO plus 20 bytes digest */
		tinfo = EMSA_SHA1_DIGESTINFO;
		tlen = 15;
	}
	else if (strcmp(ctxt->algo->name, "SHA-256") == 0)
	{
		/* tlen is 19 bytes for EMSA_SHA256_DIGESTINFO plus 32 bytes digest */
		tinfo = EMSA_SHA256_DIGESTINFO;
		tlen = 19;
	}
	else
		goto cleanup;

	tlen += digestsize;

	/* fill emdata with 0x00 0x01 0xff .... 0xff 0x00 EMSA_x_DIGESTINFO DIGEST */
	emdata[0] = 0x00;
	emdata[1] = 0x01;
	memset(emdata+2, 0xff, emlen-tlen-3);
	emdata[emlen-tlen-1] = 0x00;
	memcpy(emdata+emlen-tlen, tinfo, tlen-digestsize);

	hashFunctionContextDigest(ctxt, emdata+emlen-digestsize);

	rc = 0;

cleanup:

	return rc;
}
