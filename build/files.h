#ifndef	_H_FILES_
#define	_H_FILES_

#include "spec.h"
#include "lib/cpio.h"

#ifdef __cplusplus
extern "C" {
#endif

int processBinaryFiles(Spec spec, int installSpecialDoc, int test);
int processSourceFiles(Spec spec);
void freeCpioList(struct cpioFileMapping *cpioList, int cpioCount);

#ifdef __cplusplus
}
#endif

#endif	/* _H_FILES_ */
