/*
 * Copyright (c) 2003 Bob Deblier
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

/*!\file benchbc.c
 * \brief Benchmark program for Block Ciphers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#include "beecrypt.h"
#include "timestamp.h"

#include <stdio.h>

#define SECONDS	10

void validnames()
{
	int i;

	for (i = 0; i < blockCipherCount(); i++)
	{
		const blockCipher* tmp = blockCipherGet(i);

		if (tmp)
			fprintf(stderr, " %s", tmp->name);
	}
	fprintf(stderr, "\n");
}

void usage()
{
	fprintf(stderr, "Usage: benchbf <blockcipher> <keybits> [<size>]\n");
	fprintf(stderr, "   <blockcipher> can be any of:");
	validnames();
	exit(1);
}

byte key[1024];

int benchmark(const blockCipher* bc, int keybits, int size)
{
	blockCipherContext bcc;

	void* cleartext = (void*) malloc(size << 10);
	void* ciphertext = (void*) malloc(size << 10);

	if (blockCipherContextInit(&bcc, bc))
	{
		fprintf(stderr, "blockCipherContextInit failed\n");
		return -1;
	}

	if (cleartext && ciphertext)
	{
		double exact, speed;
		javalong start, now;
		int iterations, nblocks;

		/* calculcate how many block we need to process */
		nblocks = (size << 10) / bc->blocksize;

		/* set up for encryption */
		if (blockCipherContextSetup(&bcc, key, keybits, ENCRYPT))
		{
			fprintf(stderr, "blockCipherContextSetup failed\n");
			return -1;
		}

		/* ECB encrypt */
		iterations = 0;
		start = timestamp();
		do
		{
			if (blockCipherContextECB(&bcc, ciphertext, cleartext, nblocks))
			{
				fprintf(stderr, "blockCipherContextECB failed\n");
				return -1;
			}

			now = timestamp();
			iterations++;
		} while (now < (start + (SECONDS * ONE_SECOND)));

		exact = (now - start);
		exact /= ONE_SECOND;

		speed = (iterations * size) / exact;

		printf("ECB encrypted %d KB in %.2f seconds = %.2f KB/s\n", iterations * size, exact, speed);

		/* CBC encrypt */
		iterations = 0;
		start = timestamp();
		do
		{
			if (blockCipherContextCBC(&bcc, ciphertext, cleartext, nblocks))
			{
				fprintf(stderr, "blockCipherContextCBC failed\n");
				return -1;
			}

			now = timestamp();
			iterations++;
		} while (now < (start + (SECONDS * ONE_SECOND)));

		exact = (now - start);
		exact /= ONE_SECOND;

		speed = (iterations * size) / exact;

		printf("CBC encrypted %d KB in %.2f seconds = %.2f KB/s\n", iterations * size, exact, speed);

		/* set up for decryption */
		if (blockCipherContextSetup(&bcc, key, keybits, DECRYPT))
		{
			fprintf(stderr, "blockCipherContextSetup failed\n");
			return -1;
		}

		/* ECB decrypt */
		iterations = 0;
		start = timestamp();
		do
		{
			if (blockCipherContextECB(&bcc, cleartext, ciphertext, nblocks))
			{
				fprintf(stderr, "blockCipherContextECB failed\n");
				return -1;
			}

			now = timestamp();
			iterations++;
		} while (now < (start + (SECONDS * ONE_SECOND)));

		exact = (now - start);
		exact /= ONE_SECOND;

		speed = (iterations * size) / exact;

		printf("ECB decrypted %d KB in %.2f seconds = %.2f KB/s\n", iterations * size, exact, speed);

		/* CBC decrypt */
		iterations = 0;
		start = timestamp();
		do
		{
			if (blockCipherContextCBC(&bcc, cleartext, ciphertext, nblocks))
			{
				fprintf(stderr, "blockCipherContextCBC failed\n");
				return -1;
			}

			now = timestamp();
			iterations++;
		} while (now < (start + (SECONDS * ONE_SECOND)));

		exact = (now - start);
		exact /= ONE_SECOND;

		speed = (iterations * size) / exact;

		printf("CBC decrypted %d KB in %.2f seconds = %.2f KB/s\n", iterations * size, exact, speed);

		free(ciphertext);
		free(cleartext);
	}

	if (blockCipherContextFree(&bcc))
	{
		fprintf(stderr, "blockCipherContextFree failed\n");
		return -1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	const blockCipher* bc;
	int keybits;
	int size = 1024;

	if (argc < 3 || argc > 5)
		usage();

	bc = blockCipherFind(argv[1]);

	keybits = atoi(argv[2]);

	if (!bc)
	{
		fprintf(stderr, "Illegal blockcipher name\n");
		usage();
	}

	if (argc == 4)
	{
		size = atoi(argv[2]);
		if (size <= 0)
			usage();
	}

	return benchmark(bc, keybits, size);
}
