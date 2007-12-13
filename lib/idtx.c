#include "system.h"

#include <rpm/idtx.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

static int reverse = -1;

/**
 */
static int IDTintcmp(const void * a, const void * b)
{
    return ( reverse * (((IDT)a)->val.u32 - ((IDT)b)->val.u32) );
}

IDTX IDTXfree(IDTX idtx)
{
    if (idtx) {
	int i;
	if (idtx->idt)
	for (i = 0; i < idtx->nidt; i++) {
	    IDT idt = idtx->idt + i;
	    idt->h = headerFree(idt->h);
	    idt->key = _free(idt->key);
	}
	idtx->idt = _free(idtx->idt);
	idtx = _free(idtx);
    }
    return NULL;
}

IDTX IDTXnew(void)
{
    IDTX idtx = xcalloc(1, sizeof(*idtx));
    idtx->delta = 10;
    idtx->size = sizeof(*((IDT)0));
    return idtx;
}

IDTX IDTXgrow(IDTX idtx, int need)
{
    if (need < 0) return NULL;
    if (idtx == NULL)
	idtx = IDTXnew();
    if (need == 0) return idtx;

    if ((idtx->nidt + need) > idtx->alloced) {
	while (need > 0) {
	    idtx->alloced += idtx->delta;
	    need -= idtx->delta;
	}
	idtx->idt = xrealloc(idtx->idt, (idtx->alloced * idtx->size) );
    }
    return idtx;
}

IDTX IDTXsort(IDTX idtx)
{
    if (idtx != NULL && idtx->idt != NULL && idtx->nidt > 0)
	qsort(idtx->idt, idtx->nidt, idtx->size, IDTintcmp);
    return idtx;
}

IDTX IDTXload(rpmts ts, rpm_tag_t tag)
{
    IDTX idtx = NULL;
    rpmdbMatchIterator mi;
    HGE_t hge = (HGE_t) headerGetEntry;
    Header h;

    mi = rpmtsInitIterator(ts, tag, NULL, 0);
#ifdef	NOTYET
    (void) rpmdbSetIteratorRE(mi, RPMTAG_NAME, RPMMIRE_DEFAULT, '!gpg-pubkey');
#endif
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	rpmTagType type = RPM_NULL_TYPE;
	rpm_count_t count = 0;
	int32_t * tidp;

	tidp = NULL;
	if (!hge(h, tag, &type, (void **)&tidp, &count) || tidp == NULL)
	    continue;

	if (type == RPM_INT32_TYPE && (*tidp == 0 || *tidp == -1))
	    continue;

	idtx = IDTXgrow(idtx, 1);
	if (idtx == NULL)
	    continue;
	if (idtx->idt == NULL)
	    continue;

	{   IDT idt;
	    idt = idtx->idt + idtx->nidt;
	    idt->h = headerLink(h);
	    idt->key = NULL;
	    idt->instance = rpmdbGetIteratorOffset(mi);
	    idt->val.u32 = *tidp;
	}
	idtx->nidt++;
    }
    mi = rpmdbFreeIterator(mi);

    return IDTXsort(idtx);
}

IDTX IDTXglob(rpmts ts, const char * globstr, rpm_tag_t tag)
{
    IDTX idtx = NULL;
    HGE_t hge = (HGE_t) headerGetEntry;
    Header h;
    int32_t * tidp;
    FD_t fd;
    const char ** av = NULL;
    int ac = 0;
    rpmRC rpmrc;
    int xx;
    int i;

    av = NULL;	ac = 0;
    xx = rpmGlob(globstr, &ac, &av);

    if (xx == 0)
    for (i = 0; i < ac; i++) {
	rpmTagType type;
	rpm_count_t count;
	int isSource;

	fd = Fopen(av[i], "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
            rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), av[i],
                        Fstrerror(fd));
	    if (fd != NULL) (void) Fclose(fd);
	    continue;
	}

	rpmrc = rpmReadPackageFile(ts, fd, av[i], &h);
	(void) Fclose(fd);
	switch (rpmrc) {
	default:
	    goto bottom;
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    isSource = headerIsSource(h);
	    if (isSource)
		goto bottom;
	    break;
	}

	tidp = NULL;
	if (hge(h, tag, &type, (void **) &tidp, &count) && tidp != NULL) {

	    idtx = IDTXgrow(idtx, 1);
	    if (idtx == NULL || idtx->idt == NULL)
		goto bottom;

	    {	IDT idt;
		idt = idtx->idt + idtx->nidt;
		idt->h = headerLink(h);
		idt->key = av[i];
		av[i] = NULL;
		idt->instance = 0;
		idt->val.u32 = *tidp;
	    }
	    idtx->nidt++;
	}
bottom:
	h = headerFree(h);
    }

    for (i = 0; i < ac; i++)
	av[i] = _free(av[i]);
    av = _free(av);	ac = 0;

    return IDTXsort(idtx);
}

