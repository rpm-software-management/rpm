#ifndef	_H_PACK_
#define	_H_PACK_

#include "spec.h"

#ifdef __cplusplus
extern "C" {
#endif

int packageBinaries(Spec spec);
int packageSources(Spec spec);

#ifdef __cplusplus
}
#endif

#endif	/* _H_PACK_ */
