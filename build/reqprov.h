#ifndef _REQPROV_H_
#define _REQPROV_H_

#include "spec.h"
#include "package.h"
#include "lib/cpio.h"

int addReqProv(Spec spec, Package pkg,
	       int flag, char *name, char *version, int index);
int generateAutoReqProv(Spec spec, Package pkg,
			struct cpioFileMapping *cpioList, int cpioCount);
void printReqs(Spec spec, Package pkg);

#endif
