#ifndef _H_RPMLEAD
#define _H_RPMLEAD

#include "rpmlib.h"

/* Other definitions went to rpmlib.h */

int writeLead(int fd, struct rpmlead *lead);
int readLead(int fd, struct rpmlead *lead);

#endif	/* _H_RPMLEAD */
