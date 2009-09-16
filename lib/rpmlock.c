
#include "system.h"

#include <libgen.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmts.h>

#include "lib/rpmlock.h"

#include "debug.h"

/* Internal interface */

#define RPMLOCK_PATH LOCALSTATEDIR "/rpm/.rpm.lock"
static const char * const rpmlock_path_default = "%{?_rpmlock_path}";
static const char * rpmlock_path = NULL;

enum {
    RPMLOCK_READ   = 1 << 0,
    RPMLOCK_WRITE  = 1 << 1,
    RPMLOCK_WAIT   = 1 << 2,
};

typedef struct {
    int fd;
    int openmode;
} * rpmlock;

static rpmlock rpmlock_new(const char *rootdir)
{
    rpmlock lock = (rpmlock) malloc(sizeof(*lock));

    /* XXX oneshot to determine path for fcntl lock. */
    if (rpmlock_path == NULL) {
	char * t = rpmGenPath(rootdir, rpmlock_path_default, NULL);
	if (t == NULL || *t == '\0' || *t == '%')
	    t = xstrdup(RPMLOCK_PATH);
	rpmlock_path = xstrdup(t);
	(void) rpmioMkpath(dirname(t), 0755, getuid(), getgid());
	t = _free(t);
    }
    if (lock != NULL) {
	mode_t oldmask = umask(022);
	lock->fd = open(rpmlock_path, O_RDWR|O_CREAT, 0644);
	(void) umask(oldmask);

	if (lock->fd == -1) {
	    lock->fd = open(rpmlock_path, O_RDONLY);
	    if (lock->fd == -1) {
		free(lock);
		lock = NULL;
	    } else {
		lock->openmode = RPMLOCK_READ;
	    }
	} else {
	    lock->openmode = RPMLOCK_WRITE | RPMLOCK_READ;
	}
    }
    return lock;
}

static void rpmlock_free(rpmlock lock)
{
    if (lock) {
	(void) close(lock->fd);
	free(lock);
    }
}

static int rpmlock_acquire(rpmlock lock, int mode)
{
    int res = 0;
    if (lock && (mode & lock->openmode)) {
	struct flock info;
	int cmd;
	if (mode & RPMLOCK_WAIT)
	    cmd = F_SETLKW;
	else
	    cmd = F_SETLK;
	if (mode & RPMLOCK_READ)
	    info.l_type = F_RDLCK;
	else
	    info.l_type = F_WRLCK;
	info.l_whence = SEEK_SET;
	info.l_start = 0;
	info.l_len = 0;
	info.l_pid = 0;
	if (fcntl(lock->fd, cmd, &info) != -1)
	    res = 1;
    }
    return res;
}

static void rpmlock_release(rpmlock lock)
{
    if (lock) {
	struct flock info;
	info.l_type = F_UNLCK;
	info.l_whence = SEEK_SET;
	info.l_start = 0;
	info.l_len = 0;
	info.l_pid = 0;
	(void) fcntl(lock->fd, F_SETLK, &info);
     }
}


/* External interface */

void *rpmtsAcquireLock(rpmts ts)
{
    const char *rootDir = rpmtsRootDir(ts);
    rpmlock lock;

    if (!rootDir || rpmtsChrootDone(ts))
	rootDir = "/";
    lock = rpmlock_new(rootDir);
    if (!lock) {
	rpmlog(RPMLOG_ERR, 
		_("can't create transaction lock on %s (%s)\n"), 
		rpmlock_path, strerror(errno));
    } else if (!rpmlock_acquire(lock, RPMLOCK_WRITE)) {
	if (lock->openmode & RPMLOCK_WRITE)
	    rpmlog(RPMLOG_WARNING,
		   _("waiting for transaction lock on %s\n"), rpmlock_path);
	if (!rpmlock_acquire(lock, RPMLOCK_WRITE|RPMLOCK_WAIT)) {
	    rpmlog(RPMLOG_ERR,
		   _("can't create transaction lock on %s (%s)\n"), 
		   rpmlock_path, strerror(errno));
	    rpmlock_free(lock);
	    lock = NULL;
	}
    }
    return lock;
}

void rpmtsFreeLock(void *lock)
{
    rpmlock_release((rpmlock)lock); /* Not really needed here. */
    rpmlock_free((rpmlock)lock);
}


