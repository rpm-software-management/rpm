/** \ingroup rpmdep
 * \file lib/rpmsx.c
 */
#include "system.h"

#include <rpmlib.h>

#define	_RPMSX_INTERNAL
#include "rpmsx.h"

#include "debug.h"

/*@unchecked@*/
int _rpmsx_debug = 0;

rpmsx XrpmsxUnlink(rpmsx sx, const char * msg, const char * fn, unsigned ln)
{
    if (sx == NULL) return NULL;
/*@-modfilesys@*/
if (_rpmsx_debug && msg != NULL)
fprintf(stderr, "--> sx %p -- %d %s at %s:%u\n", sx, sx->nrefs, msg, fn, ln);
/*@=modfilesys@*/
    sx->nrefs--;
    return NULL;
}

rpmsx XrpmsxLink(rpmsx sx, const char * msg, const char * fn, unsigned ln)
{
    if (sx == NULL) return NULL;
    sx->nrefs++;

/*@-modfilesys@*/
if (_rpmsx_debug && msg != NULL)
fprintf(stderr, "--> sx %p ++ %d %s at %s:%u\n", sx, sx->nrefs, msg, fn, ln);
/*@=modfilesys@*/

    /*@-refcounttrans@*/ return sx; /*@=refcounttrans@*/
}

rpmsx rpmsxFree(rpmsx sx)
{
    int i;

    if (sx == NULL)
	return NULL;

    if (sx->nrefs > 1)
	return rpmsxUnlink(sx, __func__);

/*@-modfilesys@*/
if (_rpmsx_debug < 0)
fprintf(stderr, "*** sx %p\t%s[%d]\n", sx, __func__, sx->Count);
/*@=modfilesys@*/

    /*@-branchstate@*/
    if (sx->Count > 0)
    for (i = 0; i < sx->Count; i++) {
	rpmsxp sxp = sx->sxp + i;
	sxp->pattern = _free(sxp->pattern);
	sxp->type = _free(sxp->type);
	sxp->context = _free(sxp->context);
    }
    /*@=branchstate@*/

    sx->sxp = _free(sx->sxp);

    (void) rpmsxUnlink(sx, __func__);
    /*@-refcounttrans -usereleased@*/
/*@-bounsxwrite@*/
    memset(sx, 0, sizeof(*sx));		/* XXX trash and burn */
/*@=bounsxwrite@*/
    sx = _free(sx);
    /*@=refcounttrans =usereleased@*/
    return NULL;
}

rpmsx rpmsxNew(const char * fn)
{
    rpmsx sx;

    sx = xcalloc(1, sizeof(*sx));
    sx->Count = 0;
    sx->i = -1;
    sx->sxp = NULL;

/*@-modfilesys@*/
if (_rpmsx_debug < 0)
fprintf(stderr, "*** sx %p\t%s[%d]\n", sx, __func__, sx->Count);
/*@=modfilesys@*/

    return rpmsxLink(sx, (sx ? __func__ : NULL));
}

int rpmsxCount(const rpmsx sx)
{
    return (sx != NULL ? sx->Count : 0);
}

int rpmsxIx(const rpmsx sx)
{
    return (sx != NULL ? sx->i : -1);
}

int rpmsxSetIx(rpmsx sx, int ix)
{
    int i = -1;

    if (sx != NULL) {
	i = sx->i;
	sx->i = ix;
    }
    return i;
}

int rpmsxNext(/*@null@*/ rpmsx sx)
	/*@modifies sx @*/
{
    int i = -1;

    if (sx != NULL && ++sx->i >= 0) {
	if (sx->i < sx->Count) {
	    i = sx->i;
	} else
	    sx->i = -1;

/*@-modfilesys @*/
if (_rpmsx_debug  < 0 && i != -1)
fprintf(stderr, "*** sx %p\t%s[%d]\n", sx, __func__, i);
/*@=modfilesys @*/

    }

    return i;
}

rpmsx rpmsxInit(/*@null@*/ rpmsx sx)
	/*@modifies sx @*/
{
    if (sx != NULL)
	sx->i = -1;
    /*@-refcounttrans@*/
    return sx;
    /*@=refcounttrans@*/
}
