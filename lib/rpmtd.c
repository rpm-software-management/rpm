#include "system.h"

#include <rpm/rpmtd.h>
#include <rpm/rpmstring.h>

#include "debug.h"

rpmtd rpmtdNew(void)
{
    rpmtd td = xmalloc(sizeof(*td));
    rpmtdReset(td);
    return td;
}

rpmtd rpmtdFree(rpmtd td)
{
    /* permit free on NULL td */
    if (td != NULL) {
	/* XXX should we free data too - a flag maybe? */
	free(td);
    }
    return NULL;
}

void rpmtdReset(rpmtd td)
{
    assert(td != NULL);

    memset(td, 0, sizeof(*td));
    td->ix = -1;
}

void rpmtdFreeData(rpmtd td)
{
    assert(td != NULL);

    if (td->freeData) {
	free(td->data);
    }
    rpmtdReset(td);
}

rpm_count_t rpmtdCount(rpmtd td)
{
    assert(td != NULL);
    return td->count;
}
