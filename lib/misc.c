/**
 * \file lib/misc.c
 */

#include "system.h"

/* just to put a marker in librpm.a */
const char * const RPMVERSION = VERSION;

#include <rpm/rpmurl.h>
#include <rpm/rpmmacro.h>	/* XXX for rpmGetPath */
#include <rpm/rpmlog.h>

#include "lib/misc.h"

#include "debug.h"

rpmRC rpmMkdirPath (const char * dpath, const char * dname)
{
    struct stat st;
    int rc;

    if ((rc = stat(dpath, &st)) < 0) {
	int ut = urlPath(dpath, NULL);
	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_UNKNOWN:
	    if (errno != ENOENT)
		break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	    rc = mkdir(dpath, 0755);
	    break;
	case URL_IS_DASH:
	case URL_IS_HKP:
	    break;
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

int dosetenv(const char * name, const char * value, int overwrite)
{
    char * a;

    if (!overwrite && getenv(name)) return 0;

    /* FIXME: this leaks memory! */
    a = xmalloc(strlen(name) + strlen(value) + sizeof("="));
    (void) stpcpy( stpcpy( stpcpy( a, name), "="), value);
    return putenv(a);
}

