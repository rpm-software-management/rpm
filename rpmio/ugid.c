/** \ingroup rpmio
 * \file rpmio/ugid.c
 */

#include "system.h"
#include "ugid.h"
#include "debug.h"

/* unameToUid(), uidTouname() and the group variants are really poorly
   implemented. They really ought to use hash tables. I just made the
   guess that most files would be owned by root or the same person/group
   who owned the last file. Those two values are cached, everything else
   is looked up via getpw() and getgr() functions.  If this performs
   too poorly I'll have to implement it properly :-( */

int unameToUid(const char * thisUname, uid_t * uid)
{
    /*@only@*/ static char * lastUname = NULL;
    static int lastUnameLen = 0;
    static int lastUnameAlloced;
    static uid_t lastUid;
    struct passwd * pwent;
    int thisUnameLen;

    if (!thisUname) {
	lastUnameLen = 0;
	return -1;
    } else if (!strcmp(thisUname, "root")) {
	*uid = 0;
	return 0;
    }

    thisUnameLen = strlen(thisUname);
    if (!lastUname || thisUnameLen != lastUnameLen ||
	strcmp(thisUname, lastUname)) {
	if (lastUnameAlloced < thisUnameLen + 1) {
	    lastUnameAlloced = thisUnameLen + 10;
	    lastUname = xrealloc(lastUname, lastUnameAlloced);	/* XXX memory leak */
	}
	strcpy(lastUname, thisUname);

	pwent = getpwnam(thisUname);
	if (!pwent) {
	    endpwent();
	    pwent = getpwnam(thisUname);
	    if (!pwent) return -1;
	}

	lastUid = pwent->pw_uid;
    }

    *uid = lastUid;

    return 0;
}

int gnameToGid(const char * thisGname, gid_t * gid)
{
    /*@only@*/ static char * lastGname = NULL;
    static int lastGnameLen = 0;
    static int lastGnameAlloced;
    static uid_t lastGid;
    int thisGnameLen;
    struct group * grent;

    if (!thisGname) {
	lastGnameLen = 0;
	return -1;
    } else if (!strcmp(thisGname, "root")) {
	*gid = 0;
	return 0;
    }

    thisGnameLen = strlen(thisGname);
    if (!lastGname || thisGnameLen != lastGnameLen ||
	strcmp(thisGname, lastGname)) {
	if (lastGnameAlloced < thisGnameLen + 1) {
	    lastGnameAlloced = thisGnameLen + 10;
	    lastGname = xrealloc(lastGname, lastGnameAlloced);	/* XXX memory leak */
	}
	strcpy(lastGname, thisGname);

	grent = getgrnam(thisGname);
	if (!grent) {
	    endgrent();
	    grent = getgrnam(thisGname);
	    if (!grent) return -1;
	}
	lastGid = grent->gr_gid;
    }

    *gid = lastGid;

    return 0;
}

char * uidToUname(uid_t uid)
{
    static int lastUid = -1;
    /*@only@*/ static char * lastUname = NULL;
    static int lastUnameLen = 0;
    struct passwd * pwent;
    int len;

    if (uid == (uid_t) -1) {
	lastUid = -1;
	return NULL;
    } else if (!uid) {
	return "root";
    } else if (uid == lastUid) {
	return lastUname;
    } else {
	pwent = getpwuid(uid);
	if (!pwent) return NULL;

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

char * gidToGname(gid_t gid)
{
    static int lastGid = -1;
    /*@only@*/ static char * lastGname = NULL;
    static int lastGnameLen = 0;
    struct group * grent;
    int len;

    if (gid == (gid_t) -1) {
	lastGid = -1;
	return NULL;
    } else if (!gid) {
	return "root";
    } else if (gid == lastGid) {
	return lastGname;
    } else {
	grent = getgrgid(gid);
	if (!grent) return NULL;

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
