#ifndef _REQPROV_H_
#define _REQPROV_H_

#include "specP.h"

int addReqProv(struct PackageRec *p, int flags,
	       char *name, char *version);
int generateAutoReqProv(Header header, struct PackageRec *p);
int processReqProv(Header h, struct PackageRec *p);

#endif _REQPROV_H_
