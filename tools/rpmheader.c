/* rpmheader: spit out the header portion of a package */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "header.h"
#include "pack.h"

int main(int argc, char **argv)
{
    int fd;
    char buffer[1024];
    Header hd;
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    read(fd, &buffer, RPM_LEAD_SIZE);
    hd = readHeader(fd);
    writeHeader(1, hd);
    
    return 0;
}
