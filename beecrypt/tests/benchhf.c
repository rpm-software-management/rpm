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

/*!\file benchhf.c
 * \brief Benchmark program for Hash Functions.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#include "beecrypt.h"
#include "timestamp.h"

#include <stdio.h>

#define SECONDS	10

void validnames()
{
	int i;

	for (i = 0; i < hashFunctionCount(); i++)
	{
		const hashFunction* tmp = hashFunctionGet(i);

		if (tmp)
			fprintf(stderr, " %s", tmp->name);
	}
	fprintf(stderr, "\n");
}

void usage()
{
	fprintf(stderr, "Usage: benchbf <hashfunction> [<size>]\n");
	fprintf(stderr, "   <hashfunction> can be any of:");
	validnames();
	exit(1);
}

int benchmark(const hashFunction* hf, int size)
{
	hashFunctionContext hfc;

	void* data = (void*) malloc(size << 10);

	if (hashFunctionContextInit(&hfc, hf))
		return -1;

	if (data)
	{
		double exact, speed;
		javalong start, now;
		int iterations = 0;

		/* get starting time */
		start = timestamp();
		do
		{
			if (hashFunctionContextUpdate(&hfc, data, size << 10))
				return -1;

			now = timestamp();
			iterations++;
		} while (now < (start + (SECONDS * ONE_SECOND)));

		exact = (now - start);
		exact /= ONE_SECOND;

		speed = (iterations * size) / exact;

		printf("hashed %d KB in %.2f seconds = %.2f KB/s\n", iterations * size, exact, speed);

		free(data);
	}

	if (hashFunctionContextFree(&hfc))
		return -1;

	return 0;
}

int main(int argc, char* argv[])
{
	const hashFunction* hf;
	int size = 1024;

	if (argc < 2 || argc > 4)
		usage();

	hf = hashFunctionFind(argv[1]);

	if (!hf)
	{
		fprintf(stderr, "Illegal hash function name\n");
		usage();
	}

	if (argc == 3)
	{
		size = atoi(argv[2]);
		if (size <= 0)
			usage();
	}

	return benchmark(hf, size);
}
