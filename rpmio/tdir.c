#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;

#define	HTTPPATH	"https://wellfleet.jbj.org/rawhide/test/"
#define	FTPPATH		"ftp://wellfleet.jbj.org/pub/rawhide/packages/test/"
#define	DIRPATH		"/var/ftp/pub/rawhide/packages/test"
static char * httppath = HTTPPATH;
static char * ftppath = FTPPATH;
static char * dirpath = DIRPATH;

static void printDir(const char * path)
{
    struct dirent * dp;
    DIR * dir;
    int xx;
    int i;

fprintf(stderr, "===== %s\n", path);
    dir = Opendir(path);
    i = 0;
    while ((dp = Readdir(dir)) != NULL) {
fprintf(stderr, "%5d (%x,%x) %x %x %s\n", i++,
(unsigned) dp->d_ino,
(unsigned) dp->d_off,
(unsigned) dp->d_reclen,
(unsigned) dp->d_type,
dp->d_name);
    }
    xx = Closedir(dir);
}

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
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
    printDir(dirpath);
    printDir(ftppath);
    printDir(httppath);

    return 0;
}
