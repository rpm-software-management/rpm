#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "miscfn.h"
#include "tread.h"

int timedRead(int fd, void * bufptr, int length) {
    int bytesRead;
    int total = 0;
    char * buf = bufptr;
    struct fd_set readSet;
    struct timeval tv;

    while  (total < length) {
	FD_ZERO(&readSet);
	FD_SET(fd, &readSet);

	tv.tv_sec = 5;			/* FIXME: this should be configurable */
	tv.tv_usec = 0;

	if (select(fd + 1, &readSet, NULL, NULL, &tv) != 1) 
	    return total;

	bytesRead = read(fd, buf + total, length - total);

	if (bytesRead < 0)
	    return bytesRead;
	else if (bytesRead == 0) 
	    return total;

	total += bytesRead;
    }

    return length;
}
