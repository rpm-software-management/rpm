/* pack.h -- final packing steps */

#ifndef _PACK_H_
#define _PACK_H_

#include "spec.h"

#define PACK_PACKAGE   1
#define PACK_NOPACKAGE 0

int packageBinaries(Spec s, char *passPhrase, int doPackage);
int packageSource(Spec s, char *passPhrase);

#endif _PACK_H_
