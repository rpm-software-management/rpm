/* rpmlead: spit out the lead portion of a package */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "rpmlead.h"

int main(int argc, char **argv)
{
    int fd;
    char buffer[1024];
    
    if (argc == 1) {
	fd = 0;
    } else {
	fd = open(argv[1], O_RDONLY, 0644);
    }

    read(fd, &buffer, RPMLEAD_SIZE);
    write(1, &buffer, RPMLEAD_SIZE);
    
    return 0;
}
