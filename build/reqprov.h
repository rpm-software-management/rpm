#ifndef _H_REQPROV_
#define _H_REQPROV_

#include "spec.h"
#include "package.h"
#include "lib/cpio.h"

#ifdef __cplusplus
extern "C" {
#endif

int addReqProv(Spec spec, Package pkg,
	       int flag, char *name, char *version, int index);
int generateAutoReqProv(Spec spec, Package pkg,
			struct cpioFileMapping *cpioList, int cpioCount);
void printReqs(Spec spec, Package pkg);

#ifdef __cplusplus
}
#endif

#endif	/* _H_REQPROV_ */
