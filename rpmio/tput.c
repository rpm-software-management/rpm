#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;

int noNeon;

#define	HTTPSPATH	"https://localhost/rawhide/toad/tput.txt"
#define	HTTPPATH	"http://localhost/rawhide/toad/tput.txt"
#define	FTPPATH		"ftp://localhost/home/test/tput.txt"
#define	DIRPATH		"file://localhost/var/ftp/tput.txt"
static char * httpspath = HTTPSPATH;
static char * httppath = HTTPPATH;
static char * ftppath = FTPPATH;
static char * dirpath = DIRPATH;

static size_t readFile(const char * path)
{
    char buf[BUFSIZ];
    size_t len = 0;
    FD_t fd;
    int xx;

    buf[0] = '\0';
fprintf(stderr, "===== Fread %s\n", path);
    fd = Fopen(path, "r.ufdio");
    if (fd != NULL) {

	len = Fread(buf, 1, sizeof(buf), fd);
	xx = Fclose(fd);
    }

    if (len > 0)
	fwrite(buf, 1, strlen(buf), stderr);

    return len;
}

static size_t writeFile(const char * path)
{
    char buf[BUFSIZ];
    size_t len = 0;
    FD_t fd;
    int xx;

    strcpy(buf, "Hello World!\n");
fprintf(stderr, "===== Fwrite %s\n", path);
    fd = Fopen(path, "w.ufdio");
    if (fd != NULL) {
	len = Fwrite(buf, 1, strlen(buf), fd);
	xx = Fclose(fd);
if (xx)
fprintf(stderr, "===> Fclose rc %d\n", xx);
    }

    if (len > 0)
	fwrite(buf, 1, strlen(buf), stderr);

    return len;
}

static int unlinkFile(const char * path)
{
fprintf(stderr, "===== Unlink %s\n", path);
    return Unlink(path);
}

static void doFile(const char * path)
{
    int xx;

fprintf(stderr, "===== %s\n", path);
#if 0
    xx = unlink("/home/toad/tput.txt");
    xx = unlink("/var/ftp/tput.txt");
    xx = unlink("/var/www/html/tput.txt");
#endif

#if 0
    xx = unlinkFile(path);
#endif
    xx = writeFile(path);
#if 0
    xx = readFile(path);
    xx = Unlink(path);

    xx = unlink("/home/toad/tput.txt");
    xx = unlink("/var/ftp/tput.txt");
    xx = unlink("/var/www/html/tput.txt");
#endif
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    int rc;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

_av_debug = -1;
_ftp_debug = -1;
_dav_debug = -1;
#if 0
    doFile(dirpath);
    doFile(ftppath);
#endif
    doFile(httppath);
#if 0
    doFile(httpspath);
#endif

/*@i@*/ urlFreeCache();

    return 0;
}
