#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "header.h"

void main(int argc, char ** argv)
{
    Header h;
    int fd;

    if (argc != 2) {
	fprintf(stderr, "dump <filespec>\n");
	exit(1);
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "cannot open %s: %s\n", argv[1], strerror(errno));
	exit(1);
    }

    h = readHeader(fd);
    if (!h) {
	fprintf(stderr, "readHeader error: %s\n", strerror(errno));
	exit(1);
    }
    close(fd);
  
    dumpHeader(h, stdout, 1);
    freeHeader(h);
}

  
