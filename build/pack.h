#ifndef _PACK_H_
#define _PACK_H_

#include "spec.h"

void markBuildTime(void);
int packageBinaries(Spec s);
int packageSource(Spec s);

#endif _PACK_H_
