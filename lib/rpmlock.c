
#include "system.h"

#include <errno.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include "lib/rpmlock.h"

#include "debug.h"

/* Internal interface */

enum {
    RPMLOCK_READ   = 1 << 0,
    RPMLOCK_WRITE  = 1 << 1,
    RPMLOCK_WAIT   = 1 << 2,
};

struct rpmlock_s {
    int fd;
    int openmode;
};

static rpmlock rpmlock_new(const char *lock_path)
{
    rpmlock lock = (rpmlock) malloc(sizeof(*lock));

    if (lock != NULL) {
	mode_t oldmask = umask(022);
	lock->fd = open(lock_path, O_RDWR|O_CREAT, 0644);
	(void) umask(oldmask);

	if (lock->fd == -1) {
	    lock->fd = open(lock_path, O_RDONLY);
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

rpmlock rpmlockAcquire(const char *lock_path, const char *descr)
{
    rpmlock lock = rpmlock_new(lock_path);
    if (!lock) {
	rpmlog(RPMLOG_ERR, _("can't create %s lock on %s (%s)\n"), 
		descr, lock_path, strerror(errno));
    } else if (!rpmlock_acquire(lock, RPMLOCK_WRITE)) {
	if (lock->openmode & RPMLOCK_WRITE)
	    rpmlog(RPMLOG_WARNING, _("waiting for %s lock on %s\n"),
		    descr, lock_path);
	if (!rpmlock_acquire(lock, RPMLOCK_WRITE|RPMLOCK_WAIT)) {
	    rpmlog(RPMLOG_ERR, _("can't create %s lock on %s (%s)\n"), 
		   descr, lock_path, strerror(errno));
	    rpmlock_free(lock);
	    lock = NULL;
	}
    }
    return lock;
}

rpmlock rpmlockFree(rpmlock lock)
{
    rpmlock_release(lock); /* Not really needed here. */
    rpmlock_free(lock);
    return NULL;
}


