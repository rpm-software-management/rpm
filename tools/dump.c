#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "header.h"
#include "rpmlib.h"
#include "intl.h"

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

    h = headerRead(fd, HEADER_MAGIC_YES);
    if (!h) {
	fprintf(stderr, "headerRead error: %s\n", strerror(errno));
	exit(1);
    }
    close(fd);
  
    headerDump(h, stdout, 1, rpmTagTable);
    headerFree(h);
}

  
