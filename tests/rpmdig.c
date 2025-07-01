#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpm/rpmcrypto.h>

static void printID(rpmDigestBundle b, int id)
{
    char *s = NULL;
    if (rpmDigestBundleFinal(b, id, (void **)&s, NULL, 1) == 0) {
	printf("%d: %s\n", id, s);
	free(s);
    }
}

int main(int argc, char *argv[])
{
    const char *AAA = "AAA";
    const char *BBB = "BBB";

    if (rpmInitCrypto())
	return EXIT_FAILURE;

    rpmDigestBundle b = rpmDigestBundleNew();
    rpmDigestBundleAddID(b, RPM_HASH_SHA256, 1, 0);
    rpmDigestBundleAddID(b, RPM_HASH_SHA256, 2, 0);
    rpmDigestBundleAddID(b, RPM_HASH_SHA256, 3, 0);
    rpmDigestBundleAddID(b, RPM_HASH_SHA512, 4, 0);
    
    rpmDigestBundleUpdateID(b, 2, BBB, strlen(BBB));
    rpmDigestBundleUpdate(b, AAA, strlen(AAA));

    for (int i = 1; i < 5; ++i)
	printID(b, i);

    rpmDigestBundleFree(b);
    rpmFreeCrypto();
    return 0;
}
