
#include "system.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <rpmlib.h>

#include "rpmts.h"

#include "rpmlock.h"

/* Internal interface */

#define RPMLOCK_FILE "/var/lib/rpm/transaction.lock"

enum {
	RPMLOCK_READ   = 1 << 0,
	RPMLOCK_WRITE  = 1 << 1,
	RPMLOCK_WAIT   = 1 << 2,
};

typedef struct {
	int fd;
	int openmode;
} rpmlock;

static rpmlock *rpmlock_new(const char *rootdir)
{
	rpmlock *lock = (rpmlock *)malloc(sizeof(rpmlock));
	if (lock) {
		mode_t oldmask = umask(022);
		char *path = (char *)malloc(strlen(rootdir)+
					    strlen(RPMLOCK_FILE));
		if (!path) {
			free(lock);
			return NULL;
		}
		sprintf(path, "%s/%s", rootdir, RPMLOCK_FILE);
		lock->fd = open(RPMLOCK_FILE, O_RDWR|O_CREAT, 0644);
		umask(oldmask);
		if (lock->fd == -1) {
			lock->fd = open(RPMLOCK_FILE, O_RDONLY);
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

static void rpmlock_free(rpmlock *lock)
{
	if (lock) {
		close(lock->fd);
		free(lock);
	}
}

static int rpmlock_acquire(rpmlock *lock, int mode)
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
		if (fcntl(lock->fd, cmd, &info) != -1)
			res = 1;
	}
	return res;
}

static void rpmlock_release(rpmlock *lock)
{
	if (lock) {
		struct flock info;
		info.l_type = F_UNLCK;
		info.l_whence = SEEK_SET;
		info.l_start = 0;
		info.l_len = 0;
		fcntl(lock->fd, F_SETLK, &info);
	}
}


/* External interface */

void *rpmtsAcquireLock(rpmts ts)
{
	const char *rootDir = rpmtsRootDir(ts);
	rpmlock *lock;
	if (!rootDir)
		rootDir = "/";
	lock = rpmlock_new(rootDir);
	if (!lock) {
		rpmMessage(RPMMESS_ERROR, _("can't create transaction lock\n"));
	} else if (!rpmlock_acquire(lock, RPMLOCK_WRITE)) {
		if (lock->openmode & RPMLOCK_WRITE)
			rpmMessage(RPMMESS_WARNING,
				   _("waiting for transaction lock\n"));
		if (!rpmlock_acquire(lock, RPMLOCK_WRITE|RPMLOCK_WAIT)) {
			rpmMessage(RPMMESS_ERROR,
				   _("can't create transaction lock\n"));
			rpmlock_free(lock);
			lock = NULL;
		}
	}
	return lock;
}

void rpmtsFreeLock(void *lock)
{
	rpmlock_release((rpmlock *)lock); /* Not really needed here. */
	rpmlock_free((rpmlock *)lock);
}


