#include <stdio.h>
#include <rpm/rpmlib.h>

int main(int argc, char *argv[])
{
    Header h = NULL;
    FD_t fd = Fopen(argv[1], "r");
    int rc = rpmReadPackageFile(NULL, fd, argv[1], &h);
    if (h != NULL) {
	char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
	printf("%s\n", nevra);
	free(nevra);
    }

    Fclose(fd);
    headerFree(h);
    return rc;
}
