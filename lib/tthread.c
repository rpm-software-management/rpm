#include "system.h"

#include <pthread.h>
#include <assert.h>
#include "rpmlib.h"
#include "rpmts.h"
#include "rpmsq.h"	/* XXX for _rpmsq_debug */
#include "rpmio.h"

#include "debug.h"

extern int _psm_debug;

static void *other_notify(const void *h,
			  const rpmCallbackType what,
			  const unsigned long amount,
			  const unsigned long total,
			  fnpyKey key,
			  rpmCallbackData data)
{
    static FD_t fd;

    fprintf(stderr, "notify %d %ld %ld\n", what, amount, total);

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
	fd = Fopen(key, "r");
	return fd;
	break;
    case RPMCALLBACK_INST_CLOSE_FILE:
	if (fd != NULL) {
	    (void) Fclose(fd);
	    fd = NULL;
	}
	break;
    default:
	break;
    }
    return NULL;
}

static void *
other_thread(void *dat)
{
    rpmts ts;
    int err;
    FD_t fd;
    Header h = NULL;

    rpmReadConfigFiles(NULL, NULL);
    ts = rpmtsCreate();
    assert(ts);
    (void) rpmtsSetRootDir(ts, "/");

    rpmIncreaseVerbosity();
    rpmIncreaseVerbosity();

    fd = Fopen(dat, "r.ufdio");
    assert(fd != NULL);
    rpmReadPackageFile(ts, fd, "other_thread", &h);
    Fclose(fd);

    err = rpmtsAddInstallElement(ts, h, dat, 1, NULL);

    err = rpmtsSetNotifyCallback(ts, other_notify, NULL);
    assert(!err);

    err = rpmtsRun(ts, NULL, RPMPROB_FILTER_REPLACEPKG);
    if(err)
	fprintf(stderr, "Run failed: %d\n", err);

    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_t pth;

    _psm_debug = 1;
    _rpmsq_debug = 1;

    rpmsqEnable(SIGINT, NULL);
    rpmsqEnable(SIGQUIT, NULL);
    rpmsqEnable(SIGCHLD, NULL);

    pthread_create(&pth, NULL, other_thread, argv[1]);

    pthread_join(pth, NULL);

    return 0;
}
