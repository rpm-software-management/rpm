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

    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    if (fd < 0) {
	fprintf(stderr, "cannot open %s: %s\n", argv[1], strerror(errno));
	exit(1);
    }

    h = readHeader(fd, HEADER_MAGIC);
    if (!h) {
	fprintf(stderr, "readHeader error: %s\n", strerror(errno));
	exit(1);
    }
    close(fd);
  
    dumpHeader(h, stdout, 1);
    freeHeader(h);
}

  
