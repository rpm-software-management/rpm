#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;

int noNeon;

#define	HTTPSPATH	"https://localhost/rawhide/test/*.rpm"
#if 0
#define	HTTPPATH	"http://localhost/rawhide/test/*.rpm"
#else
#define	HTTPPATH	"http://localhost/rawhide/*.rpm"
#endif
#define	FTPPATH		"ftp://localhost/pub/rawhide/packages/test/*.rpm"
#define	DIRPATH		"/var/ftp/pub/rawhide/packages/test/*.rpm"
static char * dirpath = DIRPATH;
static char * ftppath = FTPPATH;
static char * httppath = HTTPPATH;
static char * httpspath = HTTPSPATH;

static int Glob_error(const char *epath, int eerrno)
{
fprintf(stderr, "*** glob_error(%p,%d) path %s\n", epath, eerrno, epath);
    return 1;
}

static void printGlob(const char * path)
{
    glob_t gl;
    int rc;
    int i;

fprintf(stderr, "===== %s\n", path);
    gl.gl_pathc = 0;
    gl.gl_pathv = NULL;
    gl.gl_offs = 0;
    rc = Glob(path, 0, Glob_error, &gl);
fprintf(stderr, "*** Glob rc %d\n", rc);
    if (rc == 0)
    for (i = 0; i < gl.gl_pathc; i++)
	fprintf(stderr, "%5d %s\n", i, gl.gl_pathv[i]);
    Globfree(&gl);
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
    printGlob(dirpath);
    printGlob(ftppath);
#endif
    printGlob(httppath);
#if 0
    printGlob(httpspath);
#endif

    return 0;
}
