#include "system.h"

/* XXX rename the static inline version of fdFileno */
#define	fdFileno	_fdFileno
#include <rpmio_internal.h>
#undef	fdFileno

int fdFileno(void * cookie) {
    FD_t fd;
    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    return fd->fps[0].fdno;
}
