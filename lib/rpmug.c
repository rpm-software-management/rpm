#include "system.h"

#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "lib/misc.h"
#include "lib/rpmug.h"
#include "debug.h"

/* 
 * These really ought to use hash tables. I just made the
 * guess that most files would be owned by root or the same person/group
 * who owned the last file. Those two values are cached, everything else
 * is looked up via getpw() and getgr() functions.  If this performs
 * too poorly I'll have to implement it properly :-(
 */

int rpmugUid(const char * thisUname, uid_t * uid)
{
    struct passwd * pwent;
    FILE * f;

    if (!thisUname) {
	return -1;
    } else if (rstreq(thisUname, UID_0_USER)) {
	*uid = 0;
	return 0;
    }

    f = fopen("/etc/passwd", "r");
    if (!f) return -1;

    errno = 0;
    while ((pwent = fgetpwent(f)))
        if (rstreq(pwent->pw_name, thisUname)) break;
    fclose(f);
    if (pwent == NULL) return -1;

    *uid = pwent->pw_uid;

    return 0;
}

int rpmugGid(const char * thisGname, gid_t * gid)
{
    struct group * grent;
    FILE * f;

    if (thisGname == NULL) {
	return -1;
    } else if (rstreq(thisGname, GID_0_GROUP)) {
	*gid = 0;
	return 0;
    }

    f = fopen("/etc/group", "r");
    if (!f) return -1;

    errno = 0;
    while ((grent = fgetgrent(f)))
        if (rstreq(grent->gr_name, thisGname)) break;
    fclose(f);
    if (grent == NULL) return -1;

    *gid = grent->gr_gid;

    return 0;
}

const char * rpmugUname(uid_t uid)
{
    if (uid == (uid_t) -1) {
	return NULL;
    } else if (uid == (uid_t) 0) {
	return UID_0_USER;
    } else {
	struct passwd * pwent;
	FILE * f;

	f = fopen("/etc/passwd", "r");
	if (!f) return NULL;

	errno = 0;
	while ((pwent = fgetpwent(f)))
	    if (pwent->pw_uid == uid) break;
	fclose(f);
	if (pwent == NULL) return NULL;

	return pwent->pw_name;
    }
}

const char * rpmugGname(gid_t gid)
{
    if (gid == (gid_t) -1) {
	return NULL;
    } else if (gid == (gid_t) 0) {
	return GID_0_GROUP;
    } else {
	struct group * grent;
	FILE * f;

	f = fopen("/etc/group", "r");
	if (!f) return NULL;

	errno = 0;
	while ((grent = fgetgrent(f)))
	    if (grent->gr_gid == gid) break;
	fclose(f);
	if (grent == NULL) return NULL;

	return grent->gr_name;
    }
}

static void loadLibs(void)
{
    (void) getpwnam(UID_0_USER);
    endpwent();
    (void) getgrnam(GID_0_GROUP);
    endgrent();
}

int rpmugInit(void)
{
    static pthread_once_t libsLoaded = PTHREAD_ONCE_INIT;

    pthread_once(&libsLoaded, loadLibs);
    return 0;
}

void rpmugFree(void)
{
    rpmugUid(NULL, NULL);
    rpmugGid(NULL, NULL);
    rpmugUname(-1);
    rpmugGname(-1);
}
