#include "system.h"

#include <rpmlib.h>
#include <rpmurl.h>

#include <stdarg.h>
#include <err.h>

extern int _ftp_debug;
extern int _url_debug;
extern int _rpmio_debug;

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

const char *tmpdir = "/tmp";
const char *dio_xxxxxx = "/dio.XXXXXX";
#define	DIO_XXXXXX	alloca_strdup(dio_xxxxxx)
const char *fio_xxxxxx = "/fio.XXXXXX";
#define	FIO_XXXXXX	alloca_strdup(fio_xxxxxx)

static const char * xstrconcat(const char * arg, ...)
{
    const char *s;
    char *t, *te;
    size_t nt = 0;
    va_list ap;

    if (arg == NULL)	return xstrdup("");

    va_start(ap, arg);
    for (s = arg; s != NULL; s = va_arg(ap, const char *))
	nt += strlen(s);
    va_end(ap);

    te = t = xmalloc(nt+1);

    va_start(ap, arg);
    for (s = arg; s != NULL; s = va_arg(ap, const char *))
	te = stpcpy(te, s);
    va_end(ap);
    *te = '\0';
    return t;
}

static int doFIO(const char *ofn, const char *rfmode, const char *wfmode)
{
    FD_t fd;
    int rc = 0;
    char buf[8192];

    if ((fd = Fopen(ofn, wfmode)) == NULL)
	warn("Fopen: write %s (%s) %s\n", wfmode, rfmode, ofn);
    else if ((rc = Fwrite(ofn, sizeof(ofn[0]), strlen(ofn), fd)) != strlen(ofn))
	warn("Fwrite: write %s (%s) %s\n", wfmode, rfmode, ofn);
    else if ((rc = Fclose(fd)) != 0)
	warn("Fclose: write %s (%s) %s\n", wfmode, rfmode, ofn);
    else if ((fd = Fopen(ofn, rfmode)) == NULL)
	warn("Fopen: read %s (%s) %s\n", rfmode, wfmode, ofn);
    else if ((rc = Fread(buf, sizeof(buf[0]), sizeof(buf), fd)) != strlen(ofn))
	warn("Fread: read %s (%s) %s\n", rfmode, wfmode, ofn);
    else if ((rc = Fclose(fd)) != 0)
	warn("Fclose: read %s (%s) %s\n", rfmode, wfmode, ofn);
    else if (strcmp(ofn, buf))
	warn("Compare: write(%s) \"%s\" != read(%s) \"%s\" for %s\n", wfmode, ofn, rfmode, buf, ofn);
    else
	rc = 0;
    if (ufdio->_unlink(ofn) != 0)
	warn("Unlink: write(%s) read(%s) for %s\n", wfmode, rfmode, ofn);
    return rc;
}

static int doFile(const char * url, const char * odn, const char * ndn)
{
    const char * ofn = xstrconcat(odn, mktemp(FIO_XXXXXX), NULL);
    const char * nfn = xstrconcat(ndn, mktemp(FIO_XXXXXX), NULL);
    FD_t fd;
    int rc;

    if ((fd = Fopen(ofn, "r.ufdio")) != NULL)
	err(1, "Fopen: r  !exists %s fail\n", ofn);
 
    rc = doFIO(ofn, "r.ufdio", "w.ufdio");
    rc = doFIO(nfn, "r.ufdio", "w.ufdio");

    return rc;
}

static int doDir(const char *url)
{
    const char * odn = xstrconcat(url, tmpdir, mktemp(DIO_XXXXXX), NULL);
    const char * ndn = xstrconcat(url, tmpdir, mktemp(DIO_XXXXXX), NULL);

fprintf(stderr, "*** Rename #1 %s -> %s fail\n", ndn, odn);
    if (!ufdio->_rename(ndn, odn))
	err(1, "Rename: dir !exists %s !exists %s fail\n", ndn, odn);
fprintf(stderr, "*** Chdir #1 %s fail\n", odn);
    if (!ufdio->_chdir(odn))		err(1, "Chdir: !exists %s fail\n", odn);

fprintf(stderr, "*** Mkdir #1 %s\n", odn);
    if (ufdio->_mkdir(odn, 0755))	err(1, "Mkdir: !exists %s\n", odn);
fprintf(stderr, "*** Mkdir %s fail\n", odn);
    if (!ufdio->_mkdir(odn, 0755))	err(1, "Mkdir: exists %s fail\n", odn);

fprintf(stderr, "*** Chdir #2 %s\n", odn);
    if (ufdio->_chdir(odn))		err(1, "Chdir: exists %s\n", odn);

fprintf(stderr, "*** Rename #2 %s -> %s fail\n", ndn, odn);
    if (!ufdio->_rename(ndn, odn))
	err(1, "Rename: dir !exists %s exists %s fail\n", ndn, odn);
fprintf(stderr, "*** Rename #3 %s -> %s\n", odn, ndn);
    if (ufdio->_rename(odn, ndn))
	err(1, "Rename: dir exists %s !exists %s\n", odn, ndn);

fprintf(stderr, "*** Mkdir #2 %s\n", ndn);
    if (ufdio->_mkdir(odn, 0755))	err(1, "Mkdir: #2 !exists %s\n", odn);

fprintf(stderr, "*** Rename #4 %s -> %s fail\n", odn, ndn);
    if (!ufdio->_rename(odn, ndn))
	err(1, "Rename: dir exists %s exists %s fail\n", odn, ndn);

    doFile(url, odn, ndn);

fprintf(stderr, "*** Rmdir #1 %s\n", odn);
    if (ufdio->_rmdir(odn))		err(1, "Rmdir: exists %s\n", odn);
fprintf(stderr, "*** Rmdir #1 %s fail\n", odn);
    if (!ufdio->_rmdir(odn))		err(1, "Rmdir: !exists %s fail\n", odn);

fprintf(stderr, "*** Rmdir #3 %s\n", ndn);
    if (ufdio->_rmdir(ndn))		err(1, "Rmdir: exists %s\n", ndn);
fprintf(stderr, "*** Rmdir #4 %s fail\n", ndn);
    if (!ufdio->_rmdir(ndn))		err(1, "Rmdir: !exists %s fail\n", ndn);

    return 0;
}

static int doUrl(const char *url)
{
    int rc;

    rc = doDir(url);

    return rc;

}

int main (int argc, char * argv[])
{
    int rc;

    _ftp_debug = -1;
    _url_debug = -1;
    _rpmio_debug = -1;

    if (argc != 2) {
	fprintf(stderr, "%s: url ...\n", argv[0]);
	exit(1);
    }

    rc = doUrl(argv[1]);

    return 0;
}
