#ifndef	_H_bUILDIO_
#define	_H_BUILDIO_

/* XXX this information will move elsewhere soon */

#include "cpio.h"

typedef struct cpioSourceArchive {
    CFD_t	cpioCfd;
    int		cpioArchiveSize;
    int		cpioFdIn;
    struct cpioFileMapping *cpioList;
    int		cpioCount;
    struct rpmlead *lead;	/* XXX FIXME: exorcize lead/arch/os */
} CSA_t;

#ifdef __cplusplus
extern "C" {
#endif

int writeRPM(Header header, char *fileName, int type,
		CSA_t *csa, char *passPhrase, char **cookie);

#ifdef __cplusplus
}
#endif

#endif	/* _H_BUILDIO_ */
