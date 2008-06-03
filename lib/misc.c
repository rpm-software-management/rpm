/**
 * \file lib/misc.c
 */

#include "system.h"

/* just to put a marker in librpm.a */
const char * const RPMVERSION = VERSION;

#include <rpm/rpmlog.h>

#include "lib/misc.h"

#include "debug.h"

rpmRC rpmMkdirPath (const char * dpath, const char * dname)
{
    struct stat st;
    int rc;

    if ((rc = stat(dpath, &st)) < 0) {
	if (errno == ENOENT) {
	    rc = mkdir(dpath, 0755);
	}
	if (rc < 0) {
	    rpmlog(RPMLOG_ERR, _("cannot create %%%s %s\n"), dname, dpath);
	    return RPMRC_FAIL;
	}
    }
    if ((rc = access(dpath, W_OK))) {
	rpmlog(RPMLOG_ERR, _("cannot write to %%%s %s\n"), dname, dpath);
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

int doputenv(const char *str)
{
    char * a;

    /* FIXME: this leaks memory! */
    a = xmalloc(strlen(str) + 1);
    strcpy(a, str);
    return putenv(a);
}

