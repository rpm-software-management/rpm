/**
 * \file lib/misc.c
 */

#include "system.h"

/* just to put a marker in librpm.a */
const char * RPMVERSION = VERSION;

#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */
#include <rpmlib.h>
#include <rpmlog.h>
#include "lib/misc.h"
#include "debug.h"

rpmRC rpmMkdirPath (const char * dpath, const char * dname)
{
    struct stat st;
    int rc;

    if ((rc = Stat(dpath, &st)) < 0) {
	int ut = urlPath(dpath, NULL);
	switch (ut) {
	case URL_IS_PATH:
	case URL_IS_UNKNOWN:
	    if (errno != ENOENT)
		break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	    rc = Mkdir(dpath, 0755);
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
    if ((rc = Access(dpath, W_OK))) {
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

char * currentDirectory(void)
{
    int currDirLen = 0;
    char * currDir = NULL;

    do {
	currDirLen += 128;
	currDir = xrealloc(currDir, currDirLen);
	memset(currDir, 0, currDirLen);
    } while (getcwd(currDir, currDirLen) == NULL && errno == ERANGE);

    return currDir;
}

