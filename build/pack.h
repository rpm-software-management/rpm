#ifndef _PACK_H_
#define _PACK_H_

#include "spec.h"

#define RPM_LEAD_SIZE 78

int packageBinaries(Spec s);
int packageSource(Spec s);

#endif _PACK_H_
