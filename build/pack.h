/* pack.h -- final packing steps */

#ifndef _PACK_H_
#define _PACK_H_

#include "spec.h"

int packageBinaries(Spec s, char *passPhrase);
int packageSource(Spec s, char *passPhrase);

#endif _PACK_H_
