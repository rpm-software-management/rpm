/* rpmarchive: spit out the main archive portion of a package */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "spec.h"
#include "pack.h"
#include "header.h"

int main(int argc, char **argv)
{
    int fd;
    char buffer[1024];
    Header hd;
    int ct;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    read(fd, &buffer, RPM_LEAD_SIZE);
    hd = readHeader(fd);

    while ((ct = read(fd, &buffer, 1024))) {
	write(1, &buffer, ct);
    }
    
    return 0;
}
