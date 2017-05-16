#include "system.h"

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
    static char * lastUname = NULL;
    static size_t lastUnameLen = 0;
    static size_t lastUnameAlloced;
    static uid_t lastUid;
    struct passwd * pwent;
    size_t thisUnameLen;

    if (!thisUname) {
	lastUnameLen = 0;
	return -1;
    } else if (rstreq(thisUname, UID_0_USER)) {
	*uid = 0;
	return 0;
    }

    thisUnameLen = strlen(thisUname);
    if (lastUname == NULL || thisUnameLen != lastUnameLen ||
	!rstreq(thisUname, lastUname))
    {
	if (lastUnameAlloced < thisUnameLen + 1) {
	    lastUnameAlloced = thisUnameLen + 10;
	    lastUname = xrealloc(lastUname, lastUnameAlloced);	/* XXX memory leak */
	}
	strcpy(lastUname, thisUname);

	pwent = getpwnam(thisUname);
	if (pwent == NULL) {
	    /* FIX: shrug */
	    endpwent();
	    pwent = getpwnam(thisUname);
	    if (pwent == NULL) return -1;
	}

	lastUid = pwent->pw_uid;
    }

    *uid = lastUid;

    return 0;
}

int rpmugGid(const char * thisGname, gid_t * gid)
{
    static char * lastGname = NULL;
    static size_t lastGnameLen = 0;
    static size_t lastGnameAlloced;
    static gid_t lastGid;
    size_t thisGnameLen;
    struct group * grent;

    if (thisGname == NULL) {
	lastGnameLen = 0;
	return -1;
    } else if (rstreq(thisGname, GID_0_GROUP)) {
	*gid = 0;
	return 0;
    }

    thisGnameLen = strlen(thisGname);
    if (lastGname == NULL || thisGnameLen != lastGnameLen ||
	!rstreq(thisGname, lastGname))
    {
	if (lastGnameAlloced < thisGnameLen + 1) {
	    lastGnameAlloced = thisGnameLen + 10;
	    lastGname = xrealloc(lastGname, lastGnameAlloced);	/* XXX memory leak */
	}
	strcpy(lastGname, thisGname);

	grent = getgrnam(thisGname);
	if (grent == NULL) {
	    /* FIX: shrug */
	    endgrent();
	    grent = getgrnam(thisGname);
	    if (grent == NULL) {
		return -1;
	    }
	}
	lastGid = grent->gr_gid;
    }

    *gid = lastGid;

    return 0;
}

const char * rpmugUname(uid_t uid)
{
    static uid_t lastUid = (uid_t) -1;
    static char * lastUname = NULL;
    static size_t lastUnameLen = 0;

    if (uid == (uid_t) -1) {
	lastUid = (uid_t) -1;
	return NULL;
    } else if (uid == (uid_t) 0) {
	return UID_0_USER;
    } else if (uid == lastUid) {
	return lastUname;
    } else {
	struct passwd * pwent = getpwuid(uid);
	size_t len;

	if (pwent == NULL) return NULL;

	lastUid = uid;
	len = strlen(pwent->pw_name);
	if (lastUnameLen < len + 1) {
	    lastUnameLen = len + 20;
	    lastUname = xrealloc(lastUname, lastUnameLen);
	}
	strcpy(lastUname, pwent->pw_name);

	return lastUname;
    }
}

const char * rpmugGname(gid_t gid)
{
    static gid_t lastGid = (gid_t) -1;
    static char * lastGname = NULL;
    static size_t lastGnameLen = 0;

    if (gid == (gid_t) -1) {
	lastGid = (gid_t) -1;
	return NULL;
    } else if (gid == (gid_t) 0) {
	return GID_0_GROUP;
    } else if (gid == lastGid) {
	return lastGname;
    } else {
	struct group * grent = getgrgid(gid);
	size_t len;

	if (grent == NULL) return NULL;

	lastGid = gid;
	len = strlen(grent->gr_name);
	if (lastGnameLen < len + 1) {
	    lastGnameLen = len + 20;
	    lastGname = xrealloc(lastGname, lastGnameLen);
	}
	strcpy(lastGname, grent->gr_name);

	return lastGname;
    }
}

static void loadLibs(void)
{
    (void) getpwnam(UID_0_USER);
    endpwent();
    (void) getgrnam(GID_0_GROUP);
    endgrent();
    (void) gethostbyname("localhost");
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
