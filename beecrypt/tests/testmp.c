#include <stdio.h>

#include "beecrypt.h"
#include "mp.h"

#define INIT	0xdeadbeefU;

static const mpw z[4] = { 0U, 0U, 0U, 0U };
static const mpw f[4] = { ~((mpw) 0U), ~((mpw) 0U), ~((mpw) 0U), ~((mpw) 0U)};

int main()
{
	int i;
	mpw x[4];
	mpw y[4];

	for (i = 0; i < 4; i++)
		x[i] = INIT;

	mpcopy(4, x, z);
	for (i = 0; i < 4; i++)
	{
		if (x[i] != 0)
		{
			printf("mpcopy failed\n");
			return 1;
		}
	}

	if (!mpeq(4, x, z))
	{
		printf("mpeq failed\n");
		return 1;
	}
	if (mpne(4, x, z))
	{
		printf("mpne failed\n");
		return 1;
	}

	mpcopy(4, x, f);
	for (i = 0; i < 4; i++)
	{
		if (x[i] != ~((mpw) 0))
		{
			printf("mpcopy failed\n");
			return 1;
		}
	}

	if (!mpz(4, z) || mpz(4, f))
	{
		printf("mpz failed\n");
		return 1;
	}
	if (mpnz(4, z) || !mpnz(4, f))
	{
		printf("mpnz failed\n");
		return 1;
	}

	if (!mpeq(4, x, f))
	{
		printf("mpeq failed\n");
		return 1;
	}
	if (mpne(4, x, f))
	{
		printf("mpne failed\n");
		return 1;
	}

	return 0;
}
