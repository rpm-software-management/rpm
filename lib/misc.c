/**
 * \file lib/misc.c
 */

#include "system.h"

/* just to put a marker in librpm.a */
const char * RPMVERSION = VERSION;

#include "rpmio_internal.h"
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */
#include <rpmlib.h>
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
	    /*@fallthrough@*/
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
	    rpmError(RPMERR_CREATE, _("cannot create %%%s %s\n"), dname, dpath);
	    return RPMRC_FAIL;
	}
    }
    if ((rc = Access(dpath, W_OK))) {
	rpmError(RPMERR_CREATE, _("cannot write to %%%s %s\n"), dname, dpath);
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

/*@-bounds@*/
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

/*@-nullret@*/ /* FIX: list[i] is NULL */
    return list;
/*@=nullret@*/
}
/*@=bounds@*/

void freeSplitString(char ** list)
{
    /*@-unqualifiedtrans@*/
    list[0] = _free(list[0]);
    /*@=unqualifiedtrans@*/
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
    const char * tpmacro = "%{?_tmppath:%{_tmppath}}%{!?_tmppath:/var/tmp}";
    const char * tempfn = NULL;
    const char * tfn = NULL;
    static int _initialized = 0;
    int temput;
    FD_t fd = NULL;
    int ran;

    /*@-branchstate@*/
    if (!prefix) prefix = "";
    /*@=branchstate@*/

    /* Create the temp directory if it doesn't already exist. */
    /*@-branchstate@*/
    if (!_initialized) {
	_initialized = 1;
	tempfn = rpmGenPath(prefix, tpmacro, NULL);
	if (rpmioMkpath(tempfn, 0755, (uid_t) -1, (gid_t) -1))
	    goto errxit;
    }
    /*@=branchstate@*/

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
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	default:
	    /*@switchbreak@*/ break;
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
	    rpmError(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
	    goto errxit;
	}

	if (sb.st_nlink != 1) {
	    rpmError(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
	    goto errxit;
	}

	if (fstat(Fileno(fd), &sb2) == 0) {
	    if (sb2.st_ino != sb.st_ino || sb2.st_dev != sb.st_dev) {
		rpmError(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
		goto errxit;
	    }
	}
      }	break;
    default:
	break;
    }

    /*@-branchstate@*/
    if (fnptr)
	*fnptr = tempfn;
    else 
	tempfn = _free(tempfn);
    /*@=branchstate@*/
    *fdptr = fd;

    return 0;

errxit:
    tempfn = _free(tempfn);
    /*@-usereleased@*/
    if (fd != NULL) (void) Fclose(fd);
    /*@=usereleased@*/
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

/*
 * XXX This is a "dressed" entry to headerGetEntry to do:
 *      1) DIRNAME/BASENAME/DIRINDICES -> FILENAMES tag conversions.
 *      2) i18n lookaside (if enabled).
 */
int rpmHeaderGetEntry(Header h, int_32 tag, int_32 *type,
	void **p, int_32 *c)
{
    switch (tag) {
    case RPMTAG_OLDFILENAMES:
    {	const char ** fl = NULL;
	int count;
	rpmfiBuildFNames(h, RPMTAG_BASENAMES, &fl, &count);
	if (count > 0) {
	    *p = fl;
	    if (c)	*c = count;
	    if (type)	*type = RPM_STRING_ARRAY_TYPE;
	    return 1;
	}
	if (c)	*c = 0;
	return 0;
    }	/*@notreached@*/ break;

    case RPMTAG_GROUP:
    case RPMTAG_DESCRIPTION:
    case RPMTAG_SUMMARY:
    {	char fmt[128];
	const char * msgstr;
	const char * errstr;

	fmt[0] = '\0';
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tagName(tag)), "}\n");

	/* XXX FIXME: memory leak. */
        msgstr = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (msgstr) {
	    *p = (void *) msgstr;
	    if (type)	*type = RPM_STRING_TYPE;
	    if (c)	*c = 1;
	    return 1;
	} else {
	    if (c)	*c = 0;
	    return 0;
	}
    }	/*@notreached@*/ break;

    default:
	return headerGetEntry(h, tag, type, p, c);
	/*@notreached@*/ break;
    }
    /*@notreached@*/
}
