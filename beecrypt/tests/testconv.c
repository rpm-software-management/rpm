#include "mp.h"

void hexdump(byte* b, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		printf("%02x", b[i]);
		if ((i & 0xf) == 0xf)
			printf("\n");
	}
	if (i & 0xf)
		printf("\n");
}

int main()
{
	int rc;
	mpw x[4];
	byte o[9];

	mpsetw(4, x, 255);
	mpmultwo(4, x);
	rc = i2osp(o, 9, x, 4);

	printf("rc = %d\n", rc);
	hexdump(o, 9);

	rc = os2ip(x, 4, o, 9);
	printf("rc = %d\n", rc);
	mpprintln(4, x);

	exit(0);
}
