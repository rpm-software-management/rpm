#ifndef _H_RPMLEAD
#define _H_RPMLEAD

#include "rpmlib.h"

/* Other definitions went to rpmlib.h */

int writeLead(FD_t fd, struct rpmlead *lead);
int readLead(FD_t fd, struct rpmlead *lead);

#endif	/* _H_RPMLEAD */
