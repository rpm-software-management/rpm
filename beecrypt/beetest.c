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

#include "system.h"
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
#include "debug.h"

static const char* dsa_p = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291";
static const char* dsa_q = "c773218c737ec8ee993b4f2ded30f48edace915f";
static const char* dsa_g = "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802";
static const char* dsa_x = "2070b3223dba372fde1c0ffc7b2e3b498b260614";
static const char* dsa_y = "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333";
static const char* elg_n = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80290";

int testVectorInvMod(const dlkp_p* keypair)
{
	randomGeneratorContext rngc;

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	{
		register int rc;

		register uint32  size = keypair->param.p.size;
		register uint32* temp = (uint32*) malloc((13*size+11) * sizeof(uint32));

		mp32brndinv_w(&keypair->param.n, &rngc, temp, temp+size, temp+2*size);

		mp32bmulmod_w(&keypair->param.n, size, temp, size, temp+size, temp, temp+2*size);

		rc = mp32isone(size, temp);

		free(temp);

		randomGeneratorContextFree(&rngc);

		return rc;
	}
	return -1;
}

int testVectorExpMod(const dlkp_p* keypair)
{
	int rc;
	mp32number y;
	
	mp32nzero(&y);
	
	mp32bnpowmod(&keypair->param.p, &keypair->param.g, &keypair->x, &y);

	rc = mp32eqx(y.size, y.data, keypair->y.size, keypair->y.data);

	mp32nfree(&y);

	return rc;
}

int testVectorElGamalV1(const dlkp_p* keypair)
{
	int rc = 0;

	randomGeneratorContext rngc;

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	{
		mp32number digest, r, s;

		mp32nzero(&digest);
		mp32nzero(&r);
		mp32nzero(&s);

		mp32nsize(&digest, 5);

		rngc.rng->next(rngc.param, digest.data, digest.size);

		elgv1sign(&keypair->param.p, &keypair->param.n, &keypair->param.g, &rngc, &digest, &keypair->x, &r, &s);

		rc = elgv1vrfy(&keypair->param.p, &keypair->param.n, &keypair->param.g, &digest, &keypair->y, &r, &s);

		mp32nfree(&digest);
		mp32nfree(&r);
		mp32nfree(&s);

		randomGeneratorContextFree(&rngc);
	}
	return rc;
}

int testVectorElGamalV3(const dlkp_p* keypair)
{
	int rc = 0;

	randomGeneratorContext rngc;

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	{
		mp32number digest, r, s;

		mp32nzero(&digest);
		mp32nzero(&r);
		mp32nzero(&s);

		mp32nsize(&digest, 5);

		rngc.rng->next(rngc.param, digest.data, digest.size);

		elgv3sign(&keypair->param.p, &keypair->param.n, &keypair->param.g, &rngc, &digest, &keypair->x, &r, &s);

		rc = elgv3vrfy(&keypair->param.p, &keypair->param.n, &keypair->param.g, &digest, &keypair->y, &r, &s);

		mp32nfree(&digest);
		mp32nfree(&r);
		mp32nfree(&s);

		randomGeneratorContextFree(&rngc);
	}
	return rc;
}

int testVectorDHAES(const dlkp_p* keypair)
{
	/* try encrypting and decrypting a randomly generated message */

	int rc = 0;

	dhaes_p dh;

	/* incomplete */
	if (dhaes_pInit(&dh, &keypair->param, &blowfish, &hmacmd5, &md5, randomGeneratorDefault()) == 0)
	{
		mp32number mkey, mac;

		memchunk src, *dst, *cmp;

		/* make a random message of 2K size */
		src.size = 2048;
		src.data = (byte*) malloc(src.size);
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

		dhaes_pFree(&dh);

		return rc;
	}

	return -1;
}

int testVectorRSA()
{
	int rc = 0;

	randomGeneratorContext rngc;

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	{
		rsakp kp;
		mp32number digest, s;

		rsakpInit(&kp);
		fprintf(stdout, "making RSA CRT keypair\n");
		rsakpMake(&kp, &rngc, 32);
		fprintf(stdout, "RSA CRT keypair generated\n");

		mp32nzero(&digest);
		mp32nzero(&s);

		mp32bnrnd(&kp.n, &rngc, &digest);

		rsapri(&kp, &digest, &s);

		rc = rsavrfy((rsapk*) &kp, &digest, &s);

		mp32nfree(&digest);
		mp32nfree(&s);

		rsakpFree(&kp);

		randomGeneratorContextFree(&rngc);

		return rc;
	}
	return -1;
}

int testVectorDLDP()
{
	/* try generating dldp_p parameters, then see if the order of the generator is okay */
	randomGeneratorContext rc;
	dldp_p dp;

	memset(&dp, 0, sizeof(dldp_p));

	if (randomGeneratorContextInit(&rc, randomGeneratorDefault()) == 0)
	{
		register int result;
		mp32number gq;

		mp32nzero(&gq);

		dldp_pgoqMake(&dp, &rc, 768 >> 5, 512 >> 5, 1);

		/* we have the parameters, now see if g^q == 1 */
		mp32bnpowmod(&dp.p, &dp.g, (mp32number*) &dp.q, &gq);
		result = mp32isone(gq.size, gq.data);

		mp32nfree(&gq);
		dldp_pFree(&dp);

		randomGeneratorContextFree(&rc);

		return result;
	}
	return 0;
}

int testVectorMD5()
{
	uint32 expect[4] = { 0x90015098, 0x3cd24fb0, 0xd6963f7d, 0x28e17f72 };
	uint32 digest[4];
	md5Param param;

	md5Reset(&param);
	md5Update(&param, (const unsigned char*) "abc", 3);
	md5Digest(&param, digest);

	return mp32eq(4, expect, digest);
}

int testVectorSHA1()
{
	uint32 expect[5] = { 0xA9993E36, 0x4706816A, 0xBA3E2571, 0x7850C26C, 0x9CD0D89D };
	uint32 digest[5];
	sha1Param param;
	
	sha1Reset(&param);
	sha1Update(&param, (const unsigned char*) "abc", 3);
	sha1Digest(&param, digest);

	return mp32eq(5, expect, digest);
}

int testVectorSHA256()
{
	uint32 expect[8] = { 0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223, 0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad };
	uint32 digest[8];
	sha256Param param;
	
	sha256Reset(&param);
	sha256Update(&param, (const unsigned char*) "abc", 3);
	sha256Digest(&param, digest);

	return mp32eq(8, expect, digest);
}

uint32 keyValue[] = 
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
	
void testBlockInit(uint8* block, int length)
{
	register int i;
	for (i = 1; i <= length; i++)
		*(block++) = (uint8) i;
}

void testBlockCiphers()
{
	int i, k;

	fprintf(stdout, "\tTesting the blockciphers:\n");

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

			fprintf(stdout, "\t%s:\n", tmp->name);

			for (k = tmp->keybitsmin; k <= tmp->keybitsmax; k += tmp->keybitsinc)
			{
				fprintf(stdout, "\t\tsetup encrypt (%d bits key): ", k);
				if (tmp->setup(encrypt_param, keyValue, k, ENCRYPT) < 0)
				{
					fprintf(stdout, "failed\n");
					continue;
				}
				fprintf(stdout, "ok\n");
				fprintf(stdout, "\t\tsetup decrypt (%d bits key): ", k);
				if (tmp->setup(decrypt_param, keyValue, k, DECRYPT) < 0)
				{
					fprintf(stdout, "failed\n");
					continue;
				}
				fprintf(stdout, "ok\n");
				fprintf(stdout, "\t\tencrypt/decrypt test block: ");
				testBlockInit((uint8*) src_block, tmp->blocksize >> 2);

				blockEncrypt(tmp, encrypt_param, CBC, 2, enc_block, src_block);
				blockDecrypt(tmp, decrypt_param, CBC, 2, dec_block, enc_block);

				if (memcmp(dec_block, src_block, tmp->blocksize >> 2))
				{
					fprintf(stdout, "failed\n");
					continue;
				}
				fprintf(stdout, "ok\n");
				fprintf(stdout, "\t\tspeed measurement:\n");
				{
					#if HAVE_TIME_H
					double ttime;
					clock_t tstart, tstop;
					#endif

					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockEncrypt(tmp, encrypt_param, ECB, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					fprintf(stdout, "\t\t\tECB encrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blocksize << 3, ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockDecrypt(tmp, decrypt_param, ECB, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					fprintf(stdout, "\t\t\tECB decrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blocksize << 3, ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockEncrypt(tmp, encrypt_param, CBC, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					fprintf(stdout, "\t\t\tCBC encrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blocksize << 3, ttime);
					#endif
					#if HAVE_TIME_H
					tstart = clock();
					#endif
					blockEncrypt(tmp, decrypt_param, CBC, 1024 * 1024, spd_block, spd_block);
					#if HAVE_TIME_H
					tstop = clock();
					ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
					fprintf(stdout, "\t\t\tCBC decrypts 1M blocks of %d bits in %.3f seconds\n", tmp->blocksize << 3, ttime);
					#endif
				}
			}
			free(spd_block);
			free(dec_block);
			free(enc_block);
			free(src_block);
			free(decrypt_param);
			free(encrypt_param);
		}
	}
}

void testHashFunctions()
{
	int i, j;

	uint8* data = (uint8*) malloc(32 * 1024 * 1024);

	if (data)
	{
		hashFunctionContext hfc;

		fprintf(stdout, "\tTesting the hash functions:\n");

		for (i = 0; i < hashFunctionCount(); i++)
		{
			const hashFunction* tmp = hashFunctionGet(i);

			if (tmp)
			{
				uint8* digest = (uint8*) calloc(tmp->digestsize, 1);

				fprintf(stdout, "\t%s:\n", tmp->name);

				if (digest)
				{
					#if HAVE_TIME_H
					double ttime;
					clock_t tstart, tstop;
					#endif

					hashFunctionContextInit(&hfc, tmp);

					for (j = 0; j < 4; j++)
					{
						#if HAVE_TIME_H
						tstart = clock();
						#endif

						hfc.hash->reset(hfc.param);
						hfc.hash->update(hfc.param, data, 32 * 1024 * 1024);
						hfc.hash->digest(hfc.param, (uint32*) digest);

						#if HAVE_TIME_H
						tstop = clock();
						ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
						fprintf(stdout, "\t\thashes 32 MB in %.3f seconds\n", ttime);
						#endif
					}

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

	randomGeneratorContext rngc;

	mp32barrett p;
	mp32number tmp;
	mp32number g;
	mp32number x;
	mp32number y;

	mp32bzero(&p);
	mp32nzero(&g);
	mp32nzero(&x);
	mp32nzero(&y);
	mp32nzero(&tmp);

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	{
		int i;
		#if HAVE_TIME_H
		double ttime;
		clock_t tstart, tstop;
		#endif
		
		fprintf(stdout, "Timing modular exponentiations\n");
		fprintf(stdout, "\t(512 bits ^ 512 bits) mod 512 bits:");
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
		fprintf(stdout, "\t 100x in %.3f seconds\n", ttime);
		#endif
		fprintf(stdout, "\t(768 bits ^ 768 bits) mod 768 bits:");
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
		fprintf(stdout, "\t 100x in %.3f seconds\n", ttime);
		#endif
		fprintf(stdout, "\t(1024 bits ^ 1024 bits) mod 1024 bits:");
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
		fprintf(stdout, "\t 100x in %.3f seconds\n", ttime);
		#endif
		/* now run a test with x having 160 bits */
		mp32nsize(&x, 5);
		rngc.rng->next(rngc.param, x.data, x.size);
		fprintf(stdout, "\t(1024 bits ^ 160 bits) mod 1024 bits:");
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		for (i = 0; i < 100; i++)
			mp32bnpowmod(&p, &g, &x, &y);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		fprintf(stdout, "\t 100x in %.3f seconds\n", ttime);
		#endif
		mp32bfree(&p);
		mp32nfree(&g);
		mp32nfree(&x);
		mp32nfree(&y);
		mp32nfree(&tmp);

		randomGeneratorContextFree(&rngc);
	}
	else
		fprintf(stdout, "random generator setup problem\n");
}

void testDLParams()
{
	randomGeneratorContext rc;
	dldp_p dp;

	memset(&dp, 0, sizeof(dldp_p));

	if (randomGeneratorContextInit(&rc, randomGeneratorDefault()) == 0)
	{
		#if HAVE_TIME_H
		double ttime;
		clock_t tstart, tstop;
		#endif
		fprintf(stdout, "Generating P (768 bits) Q (512 bits) G with order Q\n");
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		dldp_pgoqMake(&dp, &rc, 768 >> 5, 512 >> 5, 1);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		fprintf(stdout, "\tdone in %.3f seconds\n", ttime);
		#endif
		fprintf(stdout, "P = "); fflush(stdout); mp32println(stdout, dp.p.size, dp.p.modl);
		fprintf(stdout, "Q = "); fflush(stdout); mp32println(stdout, dp.q.size, dp.q.modl);
		fprintf(stdout, "G = "); fflush(stdout); mp32println(stdout, dp.g.size, dp.g.data);
		dldp_pFree(&dp);

		fprintf(stdout, "Generating P (768 bits) Q (512 bits) G with order (P-1)\n");
		#if HAVE_TIME_H
		tstart = clock();
		#endif
		dldp_pgonMake(&dp, &rc, 768 >> 5, 512 >> 5);
		#if HAVE_TIME_H
		tstop = clock();
		ttime = ((double)(tstop - tstart)) / CLOCKS_PER_SEC;
		fprintf(stdout, "\tdone in %.3f seconds\n", ttime);
		#endif
		fprintf(stdout, "P = "); fflush(stdout); mp32println(stdout, dp.p.size, dp.p.modl);
		fprintf(stdout, "Q = "); fflush(stdout); mp32println(stdout, dp.q.size, dp.q.modl);
		fprintf(stdout, "G = "); fflush(stdout); mp32println(stdout, dp.g.size, dp.g.data);
		fprintf(stdout, "N = "); fflush(stdout); mp32println(stdout, dp.n.size, dp.n.modl);
		dldp_pFree(&dp);

		randomGeneratorContextFree(&rc);
	}
}

#if 0
int main()
{
	dlkp_p keypair;

	if (testVectorMD5())
		fprintf(stdout, "MD5 works!\n");
	else
		exit(1);

	if (testVectorSHA1())
		fprintf(stdout, "SHA-1 works!\n");
	else
		exit(1);

	if (testVectorSHA256())
		fprintf(stdout, "SHA-256 works!\n");
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
		fprintf(stdout, "InvMod works!\n");
	else
		exit(1);

	if (testVectorExpMod(&keypair))
		fprintf(stdout, "ExpMod works!\n");
	else
		exit(1);

	if (testVectorElGamalV1(&keypair))
		fprintf(stdout, "ElGamal v1 works!\n");
	else
		exit(1);

	if (testVectorElGamalV3(&keypair))
		fprintf(stdout, "ElGamal v3 works!\n");
	else
		exit(1);

	if (testVectorDHAES(&keypair))
		fprintf(stdout, "DHAES works!\n");
	else
		exit(1);

	dlkp_pFree(&keypair);

	if (testVectorRSA())
		fprintf(stdout, "RSA works!\n");
	else
		exit(1);
/*
	if (testVectorDLDP())
		fprintf(stdout, "dldp with generator of order q works!\n");
	else
		exit(1);
*/

	return 0;
}
#else
int main()
{
	int i, j;

	fprintf(stdout, "the beecrypt library implements:\n");
	fprintf(stdout, "\t%d entropy source%s:\n", entropySourceCount(), entropySourceCount() == 1 ? "" : "s");
	for (i = 0; i < entropySourceCount(); i++)
	{
		const entropySource* tmp = entropySourceGet(i);
		if (tmp)
			fprintf(stdout, "\t\t%s\n", tmp->name);
		else
			fprintf(stdout, "*** error: library corrupt\n");
	}
	fprintf(stdout, "\t%d random generator%s:\n", randomGeneratorCount(), randomGeneratorCount() == 1 ? "" : "s");
	for (i = 0; i < randomGeneratorCount(); i++)
	{
		const randomGenerator* tmp = randomGeneratorGet(i);
		if (tmp)
			fprintf(stdout, "\t\t%s\n", tmp->name);
		else
			fprintf(stdout, "*** error: library corrupt\n");
	}
	fprintf(stdout, "\t%d hash function%s:\n", hashFunctionCount(), hashFunctionCount() == 1 ? "" : "s");
	for (i = 0; i < hashFunctionCount(); i++)
	{
		const hashFunction* tmp = hashFunctionGet(i);
		if (tmp)
			fprintf(stdout, "\t\t%s\n", tmp->name);
		else
			fprintf(stdout, "*** error: library corrupt\n");
	}
	fprintf(stdout, "\t%d keyed hash function%s:\n", keyedHashFunctionCount(), keyedHashFunctionCount() == 1 ? "" : "s");
	for (i = 0; i < keyedHashFunctionCount(); i++)
	{
		const keyedHashFunction* tmp = keyedHashFunctionGet(i);
		if (tmp)
			fprintf(stdout, "\t\t%s\n", tmp->name);
		else
			fprintf(stdout, "*** error: library corrupt\n");
	}
	fprintf(stdout, "\t%d blockcipher%s:\n", blockCipherCount(), blockCipherCount() == 1 ? "" : "s");
	for (i = 0; i < blockCipherCount(); i++)
	{
		const blockCipher* tmp = blockCipherGet(i);
		if (tmp)
		{
			fprintf(stdout, "\t\t%s ", tmp->name);
			for (j = tmp->keybitsmin; j <= tmp->keybitsmax; j += tmp->keybitsinc)
			{
				fprintf(stdout, "%d", j);
				if (j < tmp->keybitsmax)
					fprintf(stdout, "/");
				else
					fprintf(stdout, " bit keys\n");
			}
		}
		else
			fprintf(stdout, "*** error: library corrupt\n");
	}
	testBlockCiphers();
	testHashFunctions();
	testExpMods();
	testDLParams();

	fprintf(stdout, "done\n");

	return 0;
}
#endif
