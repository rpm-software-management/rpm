#ifndef _PACK_H_
#define _PACK_H_

#include "spec.h"

void markBuildTime(void);
int packageBinaries(Spec s, char *passPhrase);
int packageSource(Spec s, char *passPhrase);
int doRmSource(Spec s);

#endif _PACK_H_
