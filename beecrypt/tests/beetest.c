/*
 * beetest.c
 *
 * BeeCrypt test and benchmark application
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

#include "beecrypt.h"
#include "blockmode.h"
#include "blowfish.h"
#include "mp32barrett.h"
#include "dhaes.h"
#include "dlkp.h"
#include "elgamal.h"
#include "fips180.h"
#include "hmacmd5.h"
#include "md5.h"
#include "rsa.h"
#include "sha256.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif

#include <stdio.h>

/*@unused@*/ /*@observer@*/
static const char* dsa_p = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291";
/*@unused@*/ /*@observer@*/
static const char* dsa_q = "c773218c737ec8ee993b4f2ded30f48edace915f";
/*@unused@*/ /*@observer@*/
static const char* dsa_g = "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802";
/*@unused@*/ /*@observer@*/
static const char* dsa_x = "2070b3223dba372fde1c0ffc7b2e3b498b260614";
/*@unused@*/ /*@observer@*/
static const char* dsa_y = "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333";
/*@unused@*/ /*@observer@*/
static const char* elg_n = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80290";

/*@unused@*/ static int testVectorInvMod(const dlkp_p* keypair)
	/*@*/
{
	randomGeneratorContext rngc;

	memset(&rngc, 0, sizeof(randomGeneratorContext));

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		register int rc;

		register uint32  size = keypair->param.p.size;
		register uint32* temp = (uint32*) malloc((8*size+6) * sizeof(uint32));

		/*@-nullpass -nullptrarith @*/ /* temp may be NULL */
		mp32brndinv_w(&keypair->param.n, &rngc, temp, temp+size, temp+2*size);

		mp32bmulmod_w(&keypair->param.n, size, temp, size, temp+size, temp, temp+2*size);

		rc = mp32isone(size, temp);

		free(temp);
		/*@=nullpass =nullptrarith @*/

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rngc);
		/*@=modobserver@*/

		return rc;
	}
	return -1;
}

/*@unused@*/ static int testVectorExpMod(const dlkp_p* keypair)
	/*@*/
{
	int rc;
	mp32number y;
	
	mp32nzero(&y);
	
	mp32bnpowmod(&keypair->param.p, &keypair->param.g, &keypair->x, &y);

	rc = mp32eqx(y.size, y.data, keypair->y.size, keypair->y.data);

	mp32nfree(&y);

	return rc;
}

/*@unused@*/ static int testVectorElGamalV1(const dlkp_p* keypair)
	/*@*/
{
	int rc = 0;

	randomGeneratorContext rngc;

	memset(&rngc, 0, sizeof(randomGeneratorContext));

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		mp32number digest, r, s;

		mp32nzero(&digest);
		mp32nzero(&r);
		mp32nzero(&s);

		mp32nsize(&digest, 5);

		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rngc.rng->next(rngc.param, digest.data, digest.size);
		/*@=noeffectuncon@*/

		(void) elgv1sign(&keypair->param.p, &keypair->param.n, &keypair->param.g, &rngc, &digest, &keypair->x, &r, &s);

		rc = elgv1vrfy(&keypair->param.p, &keypair->param.n, &keypair->param.g, &digest, &keypair->y, &r, &s);

		mp32nfree(&digest);
		mp32nfree(&r);
		mp32nfree(&s);

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rngc);
		/*@=modobserver@*/
	}
	return rc;
}

/*@unused@*/ static int testVectorElGamalV3(const dlkp_p* keypair)
	/*@*/
{
	int rc = 0;

	randomGeneratorContext rngc;

	memset(&rngc, 0, sizeof(randomGeneratorContext));

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		mp32number digest, r, s;

		mp32nzero(&digest);
		mp32nzero(&r);
		mp32nzero(&s);

		mp32nsize(&digest, 5);

		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rngc.rng->next(rngc.param, digest.data, digest.size);
		/*@=noeffectuncon@*/

		(void) elgv3sign(&keypair->param.p, &keypair->param.n, &keypair->param.g, &rngc, &digest, &keypair->x, &r, &s);

		rc = elgv3vrfy(&keypair->param.p, &keypair->param.n, &keypair->param.g, &digest, &keypair->y, &r, &s);

		mp32nfree(&digest);
		mp32nfree(&r);
		mp32nfree(&s);

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rngc);
		/*@=modobserver@*/
	}
	return rc;
}

#if 0
static int testVectorDHAES(const dlkp_p* keypair)
	/*@*/
{
	/* try encrypting and decrypting a randomly generated message */

	int rc = 0;

	dhaes_p dh;

	/* incomplete */
	if (dhaes_pInit(&dh, &keypair->param) == 0)
	{
		mp32number mkey, mac;

		memchunk src, *dst, *cmp;

		/* make a random message of 2K size */
		src.size = 2048;
		src.data = (byte*) malloc(src.size);
		/*@-nullpass@*/	/* malloc can return NULL */
		memset(src.data, 1, src.size);

		/* initialize the message key and mac */
		mp32nzero(&mkey);
		mp32nzero(&mac);

		/* encrypt the message */
		dst = dhaes_pEncrypt(&dh, &keypair->y, &mkey, &mac, &src);
		/* decrypt the message */
		cmp = dhaes_pDecrypt(&dh, &keypair->x, &mkey, &mac, dst);

		if (cmp != (memchunk*) 0)
		{
			if (src.size == cmp->size)
			{
				if (memcmp(src.data, cmp->data, src.size) == 0)
					rc = 1;
			}

			free(cmp->data);
			free(cmp);
		}

		free(dst->data);
		free(dst);
		free(src.data);
		/*@=nullpass@*/

		dhaes_pFree(&dh);

		return rc;
	}

	return -1;
}
#endif

/*@unused@*/ static int testVectorRSA(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
	int rc = 0;

	randomGeneratorContext rngc;

	memset(&rngc, 0, sizeof(randomGeneratorContext));

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		rsakp kp;
		mp32number digest, s;

		memset(&kp, 0, sizeof(rsakp));

		(void) rsakpInit(&kp);
		printf("making RSA CRT keypair\n");
		(void) rsakpMake(&kp, &rngc, 32);
		printf("RSA CRT keypair generated\n");

		mp32nzero(&digest);
		mp32nzero(&s);

		mp32bnrnd(&kp.n, &rngc, &digest);

		(void) rsapri(&kp, &digest, &s);

		rc = rsavrfy((rsapk*) &kp, &digest, &s);

		mp32nfree(&digest);
		mp32nfree(&s);

		(void) rsakpFree(&kp);

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rngc);
		/*@=modobserver@*/

		return rc;
	}
	return -1;
}

#if 0
/*@unused@*/ static int testVectorDSA(void)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
	int rc = 0;

	randomGeneratorContext rngc;

	return rc;
}
#endif

/*@unused@*/ static int testVectorDLDP(void)
	/*@*/
{
	/* try generating dldp_p parameters, then see if the order of the generator is okay */
	randomGeneratorContext rc;
	dldp_p dp;

	memset(&rc, 0, sizeof(randomGeneratorContext));
	memset(&dp, 0, sizeof(dldp_p));

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		register int result;
		mp32number gq;

		mp32nzero(&gq);

		(void) dldp_pgoqMake(&dp, &rc, 768 >> 5, 512 >> 5, 1);

		/* we have the parameters, now see if g^q == 1 */
		mp32bnpowmod(&dp.p, &dp.g, (mp32number*) &dp.q, &gq);
		result = mp32isone(gq.size, gq.data);

		mp32nfree(&gq);
		(void) dldp_pFree(&dp);

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rc);
		/*@=modobserver@*/

		return result;
	}
	return 0;
}

/*@unused@*/ static int testVectorMD5(void)
	/*@*/
{
	uint32 expect[4] = { 0x90015098, 0x3cd24fb0, 0xd6963f7d, 0x28e17f72 };
	uint32 digest[4];
	md5Param param;

	memset(&param, 0, sizeof(param));

	(void) md5Reset(&param);
	(void) md5Update(&param, (const unsigned char*) "abc", 3);
	(void) md5Digest(&param, digest);

	return mp32eq(4, expect, digest);
}

/*@unused@*/ static int testVectorSHA1(void)
	/*@*/
{
	uint32 expect[5] = { 0xA9993E36, 0x4706816A, 0xBA3E2571, 0x7850C26C, 0x9CD0D89D };
	uint32 digest[5];
	sha1Param param;
	
	memset(&param, 0, sizeof(param));

	(void) sha1Reset(&param);
	(void) sha1Update(&param, (const unsigned char*) "abc", 3);
	(void) sha1Digest(&param, digest);

	return mp32eq(5, expect, digest);
}

/*@unused@*/ static int testVectorSHA256(void)
	/*@*/
{
	uint32 expect[8] = { 0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223, 0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad };
	uint32 digest[8];
	sha256Param param;
	
	memset(&param, 0, sizeof(param));

	(void) sha256Reset(&param);
	/*@-internalglobs -mods @*/ /* noisy */
	(void) sha256Update(&param, (const unsigned char*) "abc", 3);
	(void) sha256Digest(&param, digest);
	/*@=internalglobs =mods @*/

	return mp32eq(8, expect, digest);
}

static uint32 keyValue[] = 
{
	0x00010203,
	0x04050607,
	0x08090a0b,
	0x0c0d0e0f,
	0x10111213,
	0x14151617,
	0x18191a1b,
	0x1c1d1e1f,
	0x20212223,
	0x24252627,
	0x28292a2b,
	0x2c2d2e2f,
	0x30313233,
	0x34353637,
	0x38393a3b,
	0x3c3d3e3f
};
	
static void testBlockInit(/*@out@*/ uint8* block, int length)
	/*@modifies *block @*/
{
	register int i;
	for (i = 1; i <= length; i++) {
		block++;
		*block = (uint8) i;
	}
}

static void testBlockCiphers(void)
	/*@globals fileSystem, internalState, keyValue @*/
	/*@modifies fileSystem, internalState @*/
{
	int i, k;

	printf("  Testing the blockciphers:\n");

	for (i = 0; i < blockCipherCount(); i++)
	{
		const blockCipher* tmp = blockCipherGet(i);

		if (tmp)
		{
			uint32 blockwords = tmp->blocksize >> 2;

			uint32* src_block = (uint32*) malloc(2 * blockwords * sizeof(uint32));
			uint32* enc_block = (uint32*) malloc(2 * blockwords * sizeof(uint32));
			uint32* dec_block = (uint32*) malloc(2 * blockwords * sizeof(uint32));
			uint32* spd_block = (uint32*) malloc(1024 * 1024 * blockwords * sizeof(uint32));

			void* encrypt_param = (void*) malloc(tmp->paramsize);
			void* decrypt_param = (void*) malloc(tmp->paramsize);

			if (encrypt_param)
				memset(encrypt_param, 0, tmp->paramsize);
			if (decrypt_param)
				memset(decrypt_param, 0, tmp->paramsize);

			printf("  %s:\n", tmp->name);

			/*@-nullpass@*/ /* malloc can return NULL */
			for (k = tmp->keybitsmin; k <= tmp->keybitsmax; k += tmp->keybitsinc)
			{
				printf("    setup encrypt (%d bits key): ", k);
				if (tmp->setup(encrypt_param, keyValue, k, ENCRYPT) < 0)
				{
					printf("failed\n");
					/*@innercontinue@*/ continue;
				}
				printf("ok\n");
				printf("    setup decrypt (%d bits key): ", k);
				if (tmp->setup(decrypt_param, keyValue, k, DECRYPT) < 0)
				{
					printf("failed\n");
					/*@innercontinue@*/ continue;
				}
				printf("ok\n");
				printf("    encrypt/decrypt test block: ");
				testBlockInit((uint8*) src_block, tmp->blocksize >> 2);

				(void) blockEncrypt(tmp, encrypt_param, CBC, 2, enc_block, src_block);
				(void) blockDecrypt(tmp, decrypt_param, CBC, 2, dec_block, enc_block);

				if (memcmp(dec_block, src_block, tmp->blocksize >> 2))
				{
					printf("failed\n");
					/*@innercontinue@*/ continue;
				}
				printf("ok\n");
				printf("    speed measurement:\n");
				{
					#if HAVE_TIME_H
					double ttime;
					clock_t tstart, tstop;
					#endif

					#if HAVE_TIME_H
					tstart = clock();
					#endif
					/*@-compdef@*/ /* spd_block undefined */
					(void) blockEncrypt(tmp, encrypt_param, ECB, 1024 * 1024, spd_block, spd_block);
					/*@=compdef@*/
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("      ECB encrypts 1M blocks of %d bits in %.3f seconds (%.3f MB/s)\n", (int)(tmp->blocksize << 3), ttime, (tmp->blocksize) / ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					(void) blockDecrypt(tmp, decrypt_param, ECB, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("      ECB decrypts 1M blocks of %d bits in %.3f seconds (%.3f MB/s)\n", (int)(tmp->blocksize << 3), ttime, (tmp->blocksize) / ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					(void) blockEncrypt(tmp, encrypt_param, CBC, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("      CBC encrypts 1M blocks of %d bits in %.3f seconds (%.3f MB/s)\n", (int)(tmp->blocksize << 3), ttime, (tmp->blocksize) / ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					(void) blockDecrypt(tmp, decrypt_param, CBC, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("      CBC decrypts 1M blocks of %d bits in %.3f seconds (%.3f MB/s)\n", (int)(tmp->blocksize << 3), ttime, (tmp->blocksize) / ttime);
					#endif
				}
			}
			free(spd_block);
			free(dec_block);
			free(enc_block);
			free(src_block);
			free(decrypt_param);
			free(encrypt_param);
			/*@=nullpass@*/
		}
	}
}

static void testHashFunctions(void)
	/*@globals fileSystem, internalState */
	/*@modifies fileSystem, internalState */
{
	int i, j;

	uint8* data = (uint8*) malloc(32 * 1024 * 1024);

	if (data)
	{
		hashFunctionContext hfc;

		memset(&hfc, 0, sizeof(hashFunctionContext));
		printf("  Testing the hash functions:\n");

		for (i = 0; i < hashFunctionCount(); i++)
		{
			const hashFunction* tmp = hashFunctionGet(i);

			if (tmp)
			{
				#if HAVE_TIME_H
				double ttime;
				clock_t tstart, tstop;
				#endif
				mp32number digest;

				mp32nzero(&digest);

				printf("  %s:\n", tmp->name);

				/*@-nullpass -modobserver @*/
				if (hashFunctionContextInit(&hfc, tmp) == 0)
				/*@=nullpass =modobserver @*/
				{
					for (j = 0; j < 4; j++)
					{
						#if HAVE_TIME_H
						tstart = clock();
						#endif

						/*@-compdef@*/ /* data undefined */
						(void) hashFunctionContextUpdate(&hfc, data, 32 * 1024 * 1024);
						/*@=compdef@*/
						(void) hashFunctionContextDigest(&hfc, &digest);

						#if HAVE_TIME_H
						tstop = clock();
						ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
						printf("    hashes 32 MB in %.3f seconds (%.3f MB/s)\n", ttime, 32.0 / ttime);
						#endif
					}

					/*@-modobserver@*/
					(void) hashFunctionContextFree(&hfc);
					/*@=modobserver@*/
				}

				mp32nfree(&digest);
			}
		}
		free(data);
	}
}

static void testExpMods(void)
	/*@globals fileSystem, internalState */
	/*@modifies fileSystem, internalState */
{
	/*@observer@*/ static const char* p_512 = "ffcf0a0767f18f9b659d92b9550351430737c3633dc6ae7d52445d937d8336e07a7ccdb119e9ab3e011a8f938151230e91187f84ac05c3220f335193fc5e351b";

	/*@observer@*/ static const char* p_768 = "f9c3dc0b8e199094e3e69386e01de863908348196d6ad2557065e6ba36d10412579f394d1114c954ee647c84551d52f214e1e1682a75e7074b91085cfaf20b2888aa056bf760948a0b678bc253633eccfca86556ddb90f000ef93041b0d53171";

	/*@observer@*/ static const char* p_1024 = "c615c47a56b47d869010256171ab164525f2ef4b887a4e0cdfc87043a9dd8894f2a18fa56729448e700f4b7420470b61257d11ecefa9ff518dc9fed5537ec6a9665ba73c948674320ff61b29c4cfa61e5baf47dfc1b80939e1bffb51787cc3252c4d1190a7f13d1b0f8d4aa986571ce5d4de5ecede1405e9bc0b5bf040a46d99";

	randomGeneratorContext rngc;

	mp32barrett p;
	mp32number tmp;
	mp32number g;
	mp32number x;
	mp32number y;

	memset(&rngc, 0, sizeof(randomGeneratorContext));

	mp32bzero(&p);
	mp32nzero(&g);
	mp32nzero(&x);
	mp32nzero(&y);
	mp32nzero(&tmp);

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		int i;
		#if HAVE_TIME_H
		double ttime;
		clock_t tstart, tstop;
		#endif
		
		printf("Timing modular exponentiations\n");
		printf("  (512 bits ^ 512 bits) mod 512 bits:");
		mp32nsethex(&tmp, p_512);
		mp32bset(&p, tmp.size, tmp.data);
		mp32nsize(&g, p.size);
		mp32nsize(&x, p.size);
		mp32bnrnd(&p, &rngc, &g);
		mp32bnrnd(&p, &rngc, &x);
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		for (i = 0; i < 100; i++)
			mp32bnpowmod(&p, &g, &x, &y);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		printf("   100x in %.3f seconds\n", ttime);
		#endif
		printf("  (768 bits ^ 768 bits) mod 768 bits:");
		mp32nsethex(&tmp, p_768);
		mp32bset(&p, tmp.size, tmp.data);
		mp32nsize(&g, p.size);
		mp32nsize(&x, p.size);
		mp32bnrnd(&p, &rngc, &g);
		mp32bnrnd(&p, &rngc, &x);
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		for (i = 0; i < 100; i++)
			mp32bnpowmod(&p, &g, &x, &y);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		printf("   100x in %.3f seconds\n", ttime);
		#endif
		printf("  (1024 bits ^ 1024 bits) mod 1024 bits:");
		mp32nsethex(&tmp, p_1024);
		mp32bset(&p, tmp.size, tmp.data);
		mp32nsize(&g, p.size);
		mp32nsize(&x, p.size);
		mp32bnrnd(&p, &rngc, &g);
		mp32bnrnd(&p, &rngc, &x);
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		for (i = 0; i < 100; i++)
			mp32bnpowmod(&p, &g, &x, &y);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		printf("   100x in %.3f seconds\n", ttime);
		#endif
		/* now run a test with x having 160 bits */
		mp32nsize(&x, 5);
		/*@-noeffectuncon@*/ /* LCL: ??? */
		(void) rngc.rng->next(rngc.param, x.data, x.size);
		/*@=noeffectuncon@*/
		printf("  (1024 bits ^ 160 bits) mod 1024 bits:");
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		for (i = 0; i < 100; i++)
			mp32bnpowmod(&p, &g, &x, &y);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		printf("   100x in %.3f seconds\n", ttime);
		#endif
		mp32bfree(&p);
		mp32nfree(&g);
		mp32nfree(&x);
		mp32nfree(&y);
		mp32nfree(&tmp);

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rngc);
		/*@=modobserver@*/
	}
	else
		printf("random generator setup problem\n");
}

static void testDLParams(void)
	/*@globals fileSystem, internalState */
	/*@modifies fileSystem, internalState */
{
	randomGeneratorContext rc;
	dldp_p dp;

	memset(&rc, 0, sizeof(randomGeneratorContext));
	memset(&dp, 0, sizeof(dldp_p));

	/*@-nullpass -modobserver @*/
	if (randomGeneratorContextInit(&rc, randomGeneratorDefault()) == 0)
	/*@=nullpass =modobserver @*/
	{
		#if HAVE_TIME_H
		double ttime;
		clock_t tstart, tstop;
		#endif
		printf("Generating P (768 bits) Q (512 bits) G with order Q\n");
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		(void) dldp_pgoqMake(&dp, &rc, 768 >> 5, 512 >> 5, 1);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		printf("  done in %.3f seconds\n", ttime);
		#endif
		printf("P = "); (void) fflush(stdout); mp32println(dp.p.size, dp.p.modl);
		printf("Q = "); (void) fflush(stdout); mp32println(dp.q.size, dp.q.modl);
		printf("G = "); (void) fflush(stdout); mp32println(dp.g.size, dp.g.data);
		(void) dldp_pFree(&dp);

		printf("Generating P (768 bits) Q (512 bits) G with order (P-1)\n");
		#if HAVE_TIME_H
		tstart = clock();
		#endif

		/*@-usereleased -compdef @*/ /* annotate dldp_pgonMake correctly */
		(void) dldp_pgonMake(&dp, &rc, 768 >> 5, 512 >> 5);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		printf("  done in %.3f seconds\n", ttime);
		#endif
		printf("P = "); (void) fflush(stdout); mp32println(dp.p.size, dp.p.modl);
		printf("Q = "); (void) fflush(stdout); mp32println(dp.q.size, dp.q.modl);
		printf("G = "); (void) fflush(stdout); mp32println(dp.g.size, dp.g.data);
		printf("N = "); (void) fflush(stdout); mp32println(dp.n.size, dp.n.modl);
		(void) dldp_pFree(&dp);
		/*@=usereleased =compdef @*/

		/*@-modobserver@*/
		(void) randomGeneratorContextFree(&rc);
		/*@=modobserver@*/
	}
}

#if 0
int main(/*@unused@*/int argc, /*@unused@*/char *argv[])
{
	dlkp_p keypair;

	if (testVectorMD5())
		printf("MD5 works!\n");
	else
		exit(1);

	if (testVectorSHA1())
		printf("SHA-1 works!\n");
	else
		exit(1);

	if (testVectorSHA256())
		printf("SHA-256 works!\n");
	else
		exit(1);

	dlkp_pInit(&keypair);

	mp32bsethex(&keypair.param.p, dsa_p);
	mp32bsethex(&keypair.param.q, dsa_q);
	mp32nsethex(&keypair.param.g, dsa_g);
	mp32bsethex(&keypair.param.n, elg_n);
	mp32nsethex(&keypair.y, dsa_y);
	mp32nsethex(&keypair.x, dsa_x);

	if (testVectorInvMod(&keypair))
		printf("InvMod works!\n");
	else
		exit(1);

	if (testVectorExpMod(&keypair))
		printf("ExpMod works!\n");
	else
		exit(1);

	if (testVectorElGamalV1(&keypair))
		printf("ElGamal v1 works!\n");
	else
		exit(1);

	if (testVectorElGamalV3(&keypair))
		printf("ElGamal v3 works!\n");
	else
		exit(1);

/*
	if (testVectorDHAES(&keypair))
		printf("DHAES works!\n");
	else
		exit(1);
*/

	dlkp_pFree(&keypair);

	if (testVectorRSA())
		printf("RSA works!\n");
	else
		exit(1);
/*
	if (testVectorDLDP())
		printf("dldp with generator of order q works!\n");
	else
		exit(1);
*/

	return 0;
}
#else
int main(/*@unused@*/int argc, /*@unused@*/char *argv[])
	/*@globals fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
	dlkp_p keypair;

	int i, j;

	printf("the beecrypt library implements:\n");
	printf("  %d entropy source%s:\n", entropySourceCount(), entropySourceCount() == 1 ? "" : "s");
	for (i = 0; i < entropySourceCount(); i++)
	{
		const entropySource* tmp = entropySourceGet(i);
		if (tmp)
			printf("    %s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("  %d random generator%s:\n", randomGeneratorCount(), randomGeneratorCount() == 1 ? "" : "s");
	for (i = 0; i < randomGeneratorCount(); i++)
	{
		const randomGenerator* tmp = randomGeneratorGet(i);
		if (tmp)
			printf("    %s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("  %d hash function%s:\n", hashFunctionCount(), hashFunctionCount() == 1 ? "" : "s");
	for (i = 0; i < hashFunctionCount(); i++)
	{
		const hashFunction* tmp = hashFunctionGet(i);
		if (tmp)
			printf("    %s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("  %d keyed hash function%s:\n", keyedHashFunctionCount(), keyedHashFunctionCount() == 1 ? "" : "s");
	for (i = 0; i < keyedHashFunctionCount(); i++)
	{
		const keyedHashFunction* tmp = keyedHashFunctionGet(i);
		if (tmp)
			printf("    %s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("  %d blockcipher%s:\n", blockCipherCount(), blockCipherCount() == 1 ? "" : "s");
	for (i = 0; i < blockCipherCount(); i++)
	{
		const blockCipher* tmp = blockCipherGet(i);
		if (tmp)
		{
			printf("    %s ", tmp->name);
			for (j = tmp->keybitsmin; j <= tmp->keybitsmax; j += tmp->keybitsinc)
			{
				printf("%d", j);
				if (j < tmp->keybitsmax)
					printf("/");
				else
					printf(" bit keys\n");
			}
		}
		else
			printf("*** error: library corrupt\n");
	}
	/*@-modnomods@*/ /* LCL: ??? */
	testBlockCiphers();
	testHashFunctions();
	testExpMods();
	testDLParams();
	/*@=modnomods@*/

	if (testVectorMD5())
		printf("MD5 works!\n");
	else
		exit(EXIT_FAILURE);

	if (testVectorSHA1())
		printf("SHA-1 works!\n");
	else
		exit(EXIT_FAILURE);

	if (testVectorSHA256())
		printf("SHA-256 works!\n");
	else
		exit(EXIT_FAILURE);

	/*@-compdef@*/
	(void) dlkp_pInit(&keypair);

	mp32bsethex(&keypair.param.p, dsa_p);
	mp32bsethex(&keypair.param.q, dsa_q);
	mp32nsethex(&keypair.param.g, dsa_g);
	mp32bsethex(&keypair.param.n, elg_n);
	mp32nsethex(&keypair.y, dsa_y);
	mp32nsethex(&keypair.x, dsa_x);

	if (testVectorInvMod(&keypair))
		printf("InvMod works!\n");
	else
		exit(EXIT_FAILURE);

	if (testVectorExpMod(&keypair))
		printf("ExpMod works!\n");
	else
		exit(EXIT_FAILURE);

	if (testVectorElGamalV1(&keypair))
		printf("ElGamal v1 works!\n");
	else
		exit(EXIT_FAILURE);

	if (testVectorElGamalV3(&keypair))
		printf("ElGamal v3 works!\n");
	else
		exit(EXIT_FAILURE);

#if 0
	if (testVectorDHAES(&keypair))
		printf("DHAES works!\n");
	else
		exit(EXIT_FAILURE);
#endif

	(void) dlkp_pFree(&keypair);
	/*@=compdef@*/

	if (testVectorRSA())
		printf("RSA works!\n");
	else
		exit(EXIT_FAILURE);
#if 1
	if (testVectorDLDP())
		printf("dldp with generator of order q works!\n");
	else
		exit(EXIT_FAILURE);
#endif

	printf("done\n");

	return 0;
}
#endif
