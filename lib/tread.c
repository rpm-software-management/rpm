#include "system.h"

#include <rpmio.h>

int timedRead(FD_t fd, void * bufptr, int length) {
    int bytesRead;
    int total = 0;
    char * buf = bufptr;
    fd_set readSet;
    struct timeval tv;
    struct stat sb;

    fstat(fdFileno(fd), &sb);
    if (S_ISREG(sb.st_mode))
	return Fread(buf, length, 1, fd);

    while  (total < length) {
	FD_ZERO(&readSet);
	FD_SET(fdFileno(fd), &readSet);

	tv.tv_sec = 30;			/* FIXME: this should be configurable */
	tv.tv_usec = 0;

	if (select(fdFileno(fd) + 1, &readSet, NULL, NULL, &tv) != 1) 
	    return total;

	bytesRead = Fread(buf + total, length - total, 1, fd);

	if (bytesRead < 0)
	    return bytesRead;
	else if (bytesRead == 0) 
	    return total;

	total += bytesRead;
    }

    return length;
}
