/**
 * \file lib/misc.c
 */

#include "system.h"

/* just to put a marker in librpm.a */
const char * RPMVERSION = VERSION;

#include "rpmurl.h"
#include <rpmmacro.h>	/* XXX for rpmGetPath */
#include <rpmlib.h>
#include "rpmerr.h"
#include "legacy.h"
#include "misc.h"
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
	    rpmlog(RPMERR_CREATE, _("cannot create %%%s %s\n"), dname, dpath);
	    return RPMRC_FAIL;
	}
    }
    if ((rc = Access(dpath, W_OK))) {
	rpmlog(RPMERR_CREATE, _("cannot write to %%%s %s\n"), dname, dpath);
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

char ** splitString(const char * str, int length, char sep)
{
    const char * source;
    char * s, * dest;
    char ** list;
    int i;
    int fields;

    s = xmalloc(length + 1);

    fields = 1;
    for (source = str, dest = s, i = 0; i < length; i++, source++, dest++) {
	*dest = *source;
	if (*dest == sep) fields++;
    }

    *dest = '\0';

    list = xmalloc(sizeof(*list) * (fields + 1));

    dest = s;
    list[0] = dest;
    i = 1;
    while (i < fields) {
	if (*dest == sep) {
	    list[i++] = dest + 1;
	    *dest = 0;
	}
	dest++;
    }

    list[i] = NULL;

/* FIX: list[i] is NULL */
    return list;
}

void freeSplitString(char ** list)
{
    list[0] = _free(list[0]);
    list = _free(list);
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

int makeTempFile(const char * prefix, const char ** fnptr, FD_t * fdptr)
{
    const char * tpmacro = "%{?_tmppath:%{_tmppath}}%{!?_tmppath:" LOCALSTATEDIR "/tmp}";
    const char * tempfn = NULL;
    const char * tfn = NULL;
    static int _initialized = 0;
    int temput;
    FD_t fd = NULL;
    int ran;

    if (!prefix) prefix = "";

    /* Create the temp directory if it doesn't already exist. */
    if (!_initialized) {
	_initialized = 1;
	tempfn = rpmGenPath(prefix, tpmacro, NULL);
	if (rpmioMkpath(tempfn, 0755, (uid_t) -1, (gid_t) -1))
	    goto errxit;
    }

    /* XXX should probably use mkstemp here */
    srand(time(NULL));
    ran = rand() % 100000;

    /* maybe this should use link/stat? */

    do {
	char tfnbuf[64];
#ifndef	NOTYET
	sprintf(tfnbuf, "rpm-tmp.%d", ran++);
	tempfn = _free(tempfn);
	tempfn = rpmGenPath(prefix, tpmacro, tfnbuf);
#else
	strcpy(tfnbuf, "rpm-tmp.XXXXXX");
	tempfn = _free(tempfn);
	tempfn = rpmGenPath(prefix, tpmacro, mktemp(tfnbuf));
#endif

	temput = urlPath(tempfn, &tfn);
	if (*tfn == '\0') goto errxit;

	switch (temput) {
	case URL_IS_DASH:
	case URL_IS_HKP:
	    goto errxit;
	    break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	default:
	    break;
	}

	fd = Fopen(tempfn, "w+x.ufdio");
	/* XXX FIXME: errno may not be correct for ufdio */
    } while ((fd == NULL || Ferror(fd)) && errno == EEXIST);

    if (fd == NULL || Ferror(fd))
	goto errxit;

    switch(temput) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
      {	struct stat sb, sb2;
	if (!stat(tfn, &sb) && S_ISLNK(sb.st_mode)) {
	    rpmlog(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
	    goto errxit;
	}

	if (sb.st_nlink != 1) {
	    rpmlog(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
	    goto errxit;
	}

	if (fstat(Fileno(fd), &sb2) == 0) {
	    if (sb2.st_ino != sb.st_ino || sb2.st_dev != sb.st_dev) {
		rpmlog(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
		goto errxit;
	    }
	}
      }	break;
    default:
	break;
    }

    if (fnptr)
	*fnptr = tempfn;
    else 
	tempfn = _free(tempfn);
    *fdptr = fd;

    return 0;

errxit:
    tempfn = _free(tempfn);
    if (fd != NULL) (void) Fclose(fd);
    return 1;
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

