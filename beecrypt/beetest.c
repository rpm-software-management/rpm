/*
 * beetest.c
 *
 * BeeCrypt test and benchmark application
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

#include "beecrypt.h"
#include "blockmode.h"
#include "mp32barrett.h"
#include "dldp.h"
#include "fips180.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_TIME_H
#include <time.h>
#endif

#include <stdio.h>

static const char* dsa_p = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291";
static const char* dsa_q = "c773218c737ec8ee993b4f2ded30f48edace915f";
static const char* dsa_g = "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802";
static const char* dsa_x = "2070b3223dba372fde1c0ffc7b2e3b498b260614";
static const char* dsa_y = "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333";

int testVectorExpMod()
{
	mp32barrett p;
	mp32number g;
	mp32number x;
	mp32number y;
	
	mp32number tmp;
	
	mp32bzero(&p);
	mp32nzero(&g);
	mp32nzero(&x);
	mp32nzero(&y);
	
	mp32nzero(&tmp);
	
	mp32nsethex(&tmp, dsa_p);
	
	mp32bset(&p, tmp.size, tmp.data);
	
	mp32nsethex(&g, dsa_g);
	mp32nsethex(&x, dsa_x);
	
	mp32bnpowmod(&p, &g, &x);

	mp32nset(&y, p.size, p.data);
	
	mp32nsethex(&tmp, dsa_y);

	return mp32eqx(y.size, y.data, tmp.size, tmp.data);
}

int testVectorSHA()
{
	uint32 expect[5] = { 0xA9993E36, 0x4706816A, 0xBA3E2571, 0x7850C26C, 0x9CD0D89D };
	uint32 digest[5];
	sha1Param param;
	
	sha1Reset(&param);
	sha1Update(&param, (const unsigned char *) "abc", 3);
	sha1Digest(&param, digest);

	return mp32eq(5, expect, digest);
}

void testBlockInit(uint8* block, int length)
{
	register int i;
	for (i = 1; i <= length; i++)
		*(block++) = (uint8) i;
}

void testBlockCiphers()
{
	int i, k;

	printf("\tTesting the blockciphers:\n");

	for (i = 0; i < blockCipherCount(); i++)
	{
		const blockCipher* tmp = blockCipherGet(i);

		if (tmp)
		{
			uint32 blockwords = tmp->blockbits >> 5;

			uint32* src_block = (uint32*) malloc(blockwords * sizeof(uint32));
			uint32* dst_block = (uint32*) malloc(blockwords * sizeof(uint32));
			uint32* spd_block = (uint32*) malloc(1024 * 1024 * blockwords * sizeof(uint32));

			void* encrypt_param = (void*) malloc(tmp->paramsize);
			void* decrypt_param = (void*) malloc(tmp->paramsize);

			printf("\t%s:\n", tmp->name);

			for (k = tmp->keybitsmin; k <= tmp->keybitsmax; k += tmp->keybitsinc)
			{
				void* key = (void*) malloc(k >> 3);

				testBlockInit((uint8*) key, k >> 3);

				printf("\t\tsetup encrypt (%d bits key): ", k);
				if (tmp->setup(encrypt_param, key, k, ENCRYPT) < 0)
				{
					free(key);
					printf("failed\n");
					continue;
				}
				printf("ok\n");
				printf("\t\tsetup decrypt (%d bits key): ", k);
				if (tmp->setup(decrypt_param, key, k, DECRYPT) < 0)
				{
					free(key);
					printf("failed\n");
					continue;
				}
				printf("ok\n");
				printf("\t\tencrypt/decrypt test block: ");
				testBlockInit((uint8*) src_block, tmp->blockbits >> 3);
				memcpy(dst_block, src_block, tmp->blockbits >> 3);
				tmp->encrypt(encrypt_param, dst_block);
				/*
				for (j = 0; j < (tmp->blockbits >> 3); j++)
				{
					printf("%02x", *(((uint8*)dst_block)+j));
				}
				printf(" ");
				*/
				tmp->decrypt(decrypt_param, dst_block);
				if (memcmp(src_block, dst_block, tmp->blockbits >> 3))
				{
					free(key);
					printf("failed\n");
					continue;
				}
				free(key);
				printf("ok\n");
				printf("\t\tspeed measurement:\n");
				{
					#if HAVE_TIME_H
					double ttime;
					clock_t tstart, tstop;
					#endif

					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockEncrypt(tmp, encrypt_param, ECB, 1024 * 1024, spd_block, spd_block, 0);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("\t\t\tECB encrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blockbits, ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockDecrypt(tmp, decrypt_param, ECB, 1024 * 1024, spd_block, spd_block, 0);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("\t\t\tECB decrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blockbits, ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockEncrypt(tmp, encrypt_param, CBC, 1024 * 1024, spd_block, spd_block, 0);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("\t\t\tCBC encrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blockbits, ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockEncrypt(tmp, decrypt_param, CBC, 1024 * 1024, spd_block, spd_block, 0);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("\t\t\tCBC decrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blockbits, ttime);
					#endif
				}
			}
			free(spd_block);
			free(dst_block);
			free(src_block);
			free(decrypt_param);
			free(encrypt_param);
		}
	}
}

void testHashFunctions()
{
	int i;

	uint8* data = (uint8*) malloc(16 * 1024 * 1024);

	if (data)
	{
		hashFunctionContext hfc;

		printf("\tTesting the hash functions:\n");

		for (i = 0; i < hashFunctionCount(); i++)
		{
			const hashFunction* tmp = hashFunctionGet(i);

			if (tmp)
			{
				uint8* digest = (uint8*) malloc(tmp->digestsize);

				printf("\t%s:\n", tmp->name);

				if (digest)
				{
					#if HAVE_TIME_H
					double ttime;
					clock_t tstart, tstop;
					#endif

					hashFunctionContextInit(&hfc, tmp);

					#if HAVE_TIME_H
					tstart = clock();
					#endif
					hfc.hash->reset(hfc.param);
					hfc.hash->update(hfc.param, data, 16 * 1024 * 1024);
					hfc.hash->digest(hfc.param, (uint32*) digest);

					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("\t\thashes 16MB in %.3f seconds\n", ttime);
					#endif

					#if HAVE_TIME_H
					tstart = clock();
					#endif
					hfc.hash->reset(hfc.param);
					hfc.hash->update(hfc.param, data, 16 * 1024 * 1024);
					hfc.hash->digest(hfc.param, (uint32*) digest);

					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					printf("\t\thashes 16MB in %.3f seconds\n", ttime);
					#endif
					free(digest);
				}
				hashFunctionContextFree(&hfc);
			}
		}
	}
}

void testExpMods()
{
	static const char* p_512 = "ffcf0a0767f18f9b659d92b9550351430737c3633dc6ae7d52445d937d8336e07a7ccdb119e9ab3e011a8f938151230e91187f84ac05c3220f335193fc5e351b";

	static const char* p_768 = "f9c3dc0b8e199094e3e69386e01de863908348196d6ad2557065e6ba36d10412579f394d1114c954ee647c84551d52f214e1e1682a75e7074b91085cfaf20b2888aa056bf760948a0b678bc253633eccfca86556ddb90f000ef93041b0d53171";

	static const char* p_1024 = "c615c47a56b47d869010256171ab164525f2ef4b887a4e0cdfc87043a9dd8894f2a18fa56729448e700f4b7420470b61257d11ecefa9ff518dc9fed5537ec6a9665ba73c948674320ff61b29c4cfa61e5baf47dfc1b80939e1bffb51787cc3252c4d1190a7f13d1b0f8d4aa986571ce5d4de5ecede1405e9bc0b5bf040a46d99";

	randomGeneratorContext rc;

	mp32barrett p;
	mp32number tmp;
	mp32number g;
	mp32number x;

	mp32bzero(&p);
	mp32nzero(&g);
	mp32nzero(&x);
	mp32nzero(&tmp);

	randomGeneratorContextInit(&rc, randomGeneratorDefault());

	if (rc.rng && rc.param)
	{
		if (rc.rng->setup(rc.param) == 0)
		{
			int i;
			#if HAVE_TIME_H
			double ttime;
			clock_t tstart, tstop;
			#endif
			
			printf("Timing modular exponentiations\n");
			printf("\t(512 bits ^ 512 bits) mod 512 bits:");
			mp32nsethex(&tmp, p_512);
			mp32bset(&p, tmp.size, tmp.data);
			mp32nsize(&g, p.size);
			mp32nsize(&x, p.size);
			mp32brndres(&p, g.data, &rc);
			mp32brndres(&p, x.data, &rc);
			#if HAVE_TIME_H
			tstart = clock();
			#endif
			for (i = 0; i < 100; i++)
				mp32bnpowmod(&p, &g, &x);
			#if HAVE_TIME_H
			tstop = clock();
			ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
			printf("\t 100x in %.3f seconds\n", ttime);
			#endif
			printf("\t(768 bits ^ 768 bits) mod 768 bits:");
			mp32nsethex(&tmp, p_768);
			mp32bset(&p, tmp.size, tmp.data);
			mp32nsize(&g, p.size);
			mp32nsize(&x, p.size);
			mp32brndres(&p, g.data, &rc);
			mp32brndres(&p, x.data, &rc);
			#if HAVE_TIME_H
			tstart = clock();
			#endif
			for (i = 0; i < 100; i++)
				mp32bnpowmod(&p, &g, &x);
			#if HAVE_TIME_H
			tstop = clock();
			ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
			printf("\t 100x in %.3f seconds\n", ttime);
			#endif
			printf("\t(1024 bits ^ 1024 bits) mod 1024 bits:");
			mp32nsethex(&tmp, p_1024);
			mp32bset(&p, tmp.size, tmp.data);
			mp32nsize(&g, p.size);
			mp32nsize(&x, p.size);
			mp32brndres(&p, g.data, &rc);
			mp32brndres(&p, x.data, &rc);
			#if HAVE_TIME_H
			tstart = clock();
			#endif
			for (i = 0; i < 100; i++)
				mp32bnpowmod(&p, &g, &x);
			#if HAVE_TIME_H
			tstop = clock();
			ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
			printf("\t 100x in %.3f seconds\n", ttime);
			#endif
			/* now run a test with x having 160 bits */
			mp32nsize(&x, 5);
			rc.rng->next(rc.param, x.data, x.size);
			printf("\t(1024 bits ^ 160 bits) mod 1024 bits:");
			#if HAVE_TIME_H
			tstart = clock();
			#endif
			for (i = 0; i < 100; i++)
				mp32bnpowmod(&p, &g, &x);
			#if HAVE_TIME_H
			tstop = clock();
			ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
			printf("\t 100x in %.3f seconds\n", ttime);
			#endif
			mp32bfree(&p);
			mp32nfree(&g);
			mp32nfree(&x);
			mp32nfree(&tmp);
		}
	}

	randomGeneratorContextFree(&rc);
}

void testDLParams()
{
	randomGeneratorContext rc;
	dldp_p dp;

	memset(&dp, 0, sizeof(dldp_p));

	randomGeneratorContextInit(&rc, randomGeneratorDefault());

	if (rc.rng && rc.param)
	{
		if (rc.rng->setup(rc.param) == 0)
		{
			#if HAVE_TIME_H
			double ttime;
			clock_t tstart, tstop;
			#endif
			#if HAVE_TIME_H
			tstart = clock();
			#endif
			printf("Generating P (768 bits) Q (512 bits) G with order Q\n");
			dldp_pgoqMake(&dp, &rc, 768 >> 5, 512 >> 5, 1);
			#if HAVE_TIME_H
			tstop = clock();
			ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
			printf("\tdone in %.3f seconds\n", ttime);
			#endif
		}
	}

	randomGeneratorContextFree(&rc);
}

int main()
{

	int i, j;

	printf("the beecrypt library implements:\n");
	printf("\t%d entropy source%s:\n", entropySourceCount(), entropySourceCount() == 1 ? "" : "s");
	for (i = 0; i < entropySourceCount(); i++)
	{
		const entropySource* tmp = entropySourceGet(i);
		if (tmp)
			printf("\t\t%s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("\t%d random generator%s:\n", randomGeneratorCount(), randomGeneratorCount() == 1 ? "" : "s");
	for (i = 0; i < randomGeneratorCount(); i++)
	{
		const randomGenerator* tmp = randomGeneratorGet(i);
		if (tmp)
			printf("\t\t%s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("\t%d hash function%s:\n", hashFunctionCount(), hashFunctionCount() == 1 ? "" : "s");
	for (i = 0; i < hashFunctionCount(); i++)
	{
		const hashFunction* tmp = hashFunctionGet(i);
		if (tmp)
			printf("\t\t%s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("\t%d keyed hash function%s:\n", keyedHashFunctionCount(), keyedHashFunctionCount() == 1 ? "" : "s");
	for (i = 0; i < keyedHashFunctionCount(); i++)
	{
		const keyedHashFunction* tmp = keyedHashFunctionGet(i);
		if (tmp)
			printf("\t\t%s\n", tmp->name);
		else
			printf("*** error: library corrupt\n");
	}
	printf("\t%d blockcipher%s:\n", blockCipherCount(), blockCipherCount() == 1 ? "" : "s");
	for (i = 0; i < blockCipherCount(); i++)
	{
		const blockCipher* tmp = blockCipherGet(i);
		if (tmp)
		{
			printf("\t\t%s ", tmp->name);
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

	testBlockCiphers();
	testHashFunctions();
	testExpMods();
	testDLParams();

	printf("done\n");

	return 0;
}
