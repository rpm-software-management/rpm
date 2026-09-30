#include "system.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    struct stat sb;
    unsigned char *data = NULL;
    char needle[32768];
    size_t nlen = 0;
    size_t align;
    int fd = -1;
    int rc = 1;

    if (argc != 3 && argc != 4)
	return 2;
    if (argc == 4 && strcmp(argv[3], "content") != 0)
	return 2;
    align = strtoul(argv[2], NULL, 0);
    if (align == 0)
	return 2;

    /* nested.txt stops at 5000; including 5001 uniquely identifies big.txt
     * while retaining its alignment-constrained start as the match offset. */
    for (int i = 1; i <= 5001; i++) {
	int n = snprintf(needle + nlen, sizeof(needle) - nlen, "%d\n", i);
	if (n < 0 || (size_t)n >= sizeof(needle) - nlen)
	    return 2;
	nlen += n;
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0 || fstat(fd, &sb) || sb.st_size <= 0)
	goto exit;
    data = malloc(sb.st_size);
    if (data == NULL || pread(fd, data, sb.st_size, 0) != sb.st_size)
	goto exit;

    for (off_t pos = align; pos + (off_t)nlen <= sb.st_size; pos += align) {
	size_t zeros = 0;
	while (zeros < (size_t)pos && data[pos - zeros - 1] == '\0')
	    zeros++;
	/* Ordinary full-cpio padding plus the pathname terminator can account
	 * for at most four NULs. Require more to prove extension padding. */
	if (memcmp(data + pos, needle, nlen) == 0 && zeros > 4) {
	    unsigned char bad = 1;
	    off_t where = (argc == 4 && strcmp(argv[3], "content") == 0) ?
			  pos : pos - 1;
	    if (pwrite(fd, &bad, 1, where) == 1)
		rc = 0;
	    break;
	}
    }

exit:
    free(data);
    if (fd >= 0)
	close(fd);
    return rc;
}
