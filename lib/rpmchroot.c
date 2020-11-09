#include "system.h"
#include <sched.h>
#include <stdlib.h>
#include <fcntl.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include "lib/rpmchroot.h"
#include "lib/rpmug.h"
#include "debug.h"

int _rpm_nouserns = 0;

struct rootState_s {
    char *rootDir;
    int chrootDone;
    int cwd;
};

/* Process global chroot state */
static struct rootState_s rootState = {
   .rootDir = NULL,
   .chrootDone = 0,
   .cwd = -1,
}; 

#if defined(HAVE_UNSHARE) && defined(CLONE_NEWUSER)
/*
 * If setgroups file exists (Linux >= 3.19), we need to write "deny" to it,
 * otherwise gid_map will fail.
 */
static int deny_setgroups(void)
{
    int fd = open("/proc/self/setgroups", O_WRONLY, 0);
    int xx = -1;
    if (fd >= 0) {
	xx = write(fd, "deny\n", strlen("deny\n"));
	close (fd);
    }
    return (xx == -1);
}

static int setup_map(const char *path, unsigned int id, unsigned int oid)
{
    int xx = -1;
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
	char buf[256];
	int ret = snprintf(buf, sizeof(buf), "%u %u 1\n", id, oid);
	xx = write(fd, buf, ret);
	close (fd);
    }
    return (xx == -1);
}

/*
 * Try to become root by creating a user namespace. We don't really care
 * if this fails here because in that case chroot() will just fail as it
 * normally would.
 */
static void try_become_root(void)
{
    static int unshared = 0;
    uid_t uid = getuid();
    gid_t gid = getgid();
    if (!unshared && unshare(CLONE_NEWUSER | CLONE_NEWNS) == 0) {
	deny_setgroups();
	setup_map("/proc/self/uid_map", 0, uid);
	setup_map("/proc/self/gid_map", 0, gid);
	unshared = 1;
    }
    rpmlog(RPMLOG_DEBUG, "user ns: %d original user %d:%d current %d:%d\n",
	    unshared, uid, gid, getuid(), getgid());
}
#else
static void try_become_root(void)
{
}
#endif

int rpmChrootSet(const char *rootDir)
{
    int rc = 0;

    /* Setting same rootDir again is a no-op and not an error */
    if (rootDir && rootState.rootDir && rstreq(rootDir, rootState.rootDir))
	return 0;

    /* Resetting only permitted in neutral state */
    if (rootState.chrootDone != 0)
	return -1;

    rootState.rootDir = _free(rootState.rootDir);
    if (rootState.cwd >= 0) {
	close(rootState.cwd);
	rootState.cwd = -1;
    }

    if (rootDir != NULL) {
	rootState.rootDir = rstrdup(rootDir);
	rootState.cwd = open(".", O_RDONLY);
	if (rootState.cwd < 0) {
	    rpmlog(RPMLOG_ERR, _("Unable to open current directory: %m\n"));
	    rc = -1;
	}

	/* Force preloading of dlopen()'ed libraries before chroot */
	if (rpmugInit())
	    rc = -1;
    }

    return rc;
}

int rpmChrootIn(void)
{
    int rc = 0;

    if (rootState.rootDir == NULL || rstreq(rootState.rootDir, "/"))
	return 0;

    if (rootState.cwd < 0) {
	rpmlog(RPMLOG_ERR, _("%s: chroot directory not set\n"), __func__);
	return -1;
    }

    /* "refcounted" entry to chroot */
    if (rootState.chrootDone > 0) {
	rootState.chrootDone++;
    } else if (rootState.chrootDone == 0) {
	if (!_rpm_nouserns && getuid())
	    try_become_root();

	rpmlog(RPMLOG_DEBUG, "entering chroot %s\n", rootState.rootDir);
	if (chdir("/") == 0 && chroot(rootState.rootDir) == 0) {
	    rootState.chrootDone = 1;
	} else {
	    rpmlog(RPMLOG_ERR, _("Unable to change root directory: %m\n"));
	    rc = -1;
	}
    }
    return rc;
}

int rpmChrootOut(void)
{
    int rc = 0;
    if (rootState.rootDir == NULL || rstreq(rootState.rootDir, "/"))
	return 0;

    if (rootState.cwd < 0) {
	rpmlog(RPMLOG_ERR, _("%s: chroot directory not set\n"), __func__);
	return -1;
    }

    /* "refcounted" return from chroot */
    if (rootState.chrootDone > 1) {
	rootState.chrootDone--;
    } else if (rootState.chrootDone == 1) {
	rpmlog(RPMLOG_DEBUG, "exiting chroot %s\n", rootState.rootDir);
	if (chroot(".") == 0 && fchdir(rootState.cwd) == 0) {
	    rootState.chrootDone = 0;
	} else {
	    rpmlog(RPMLOG_ERR, _("Unable to restore root directory: %m\n"));
	    rc = -1;
	}
    }
    return rc;
}

int rpmChrootDone(void)
{
    return (rootState.chrootDone > 0);
}
