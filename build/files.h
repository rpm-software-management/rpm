#include "spec.h"
#include "package.h"
#include "lib/cpio.h"


int processBinaryFiles(Spec spec, int installSpecialDoc);
int processSourceFiles(Spec spec);
void freeCpioList(struct cpioFileMapping *cpioList, int cpioCount);
