/* names.c -- user/group name/id cache (plus hostname and buildtime) */

#include "system.h"

#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <netdb.h>

#include "names.h"
#include "rpmlib.h"
#include "messages.h"

static uid_t uids[1024];
static char *unames[1024];
static int uid_used = 0;

static gid_t gids[1024];
static char *gnames[1024];
static int gid_used = 0;
    
/*
 * getUname() takes a uid, gets the username, and creates an entry in the
 * table to hold a string containing the user name.
 */
char *getUname(uid_t uid)
{
    struct passwd *pw;
    int x;

    x = 0;
    while (x < uid_used) {
	if (uids[x] == uid) {
	    return unames[x];
	}
	x++;
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in getUname()\n");
	exit(RPMERR_INTERNAL);
    }
    
    pw = getpwuid(uid);
    uids[x] = uid;
    uid_used++;
    if (pw) {
	unames[x] = strdup(pw->pw_name);
    } else {
	unames[x] = NULL;
    }
    return unames[x];
}

/*
 * getUnameS() takes a username, gets the uid, and creates an entry in the
 * table to hold a string containing the user name.
 */
char *getUnameS(char *uname)
{
    struct passwd *pw;
    int x;

    x = 0;
    while (x < uid_used) {
	if (!strcmp(unames[x],uname)) {
	    return unames[x];
	}
	x++;
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in getUname()\n");
	exit(RPMERR_INTERNAL);
    }
    
    pw = getpwnam(uname);
    uid_used++;
    if (pw) {
        uids[x] = pw->pw_uid;
	unames[x] = strdup(pw->pw_name);
    } else {
        uids[x] = -1;
	unames[x] = strdup(uname);
    }
    return unames[x];
}

/*
 * getGname() takes a gid, gets the group name, and creates an entry in the
 * table to hold a string containing the group name.
 */
char *getGname(gid_t gid)
{
    struct group *gr;
    int x;

    x = 0;
    while (x < gid_used) {
	if (gids[x] == gid) {
	    return gnames[x];
	}
	x++;
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in getGname()\n");
	exit(RPMERR_INTERNAL);
    }
    
    gr = getgrgid(gid);
    gids[x] = gid;
    gid_used++;
    if (gr) {
	gnames[x] = strdup(gr->gr_name);
    } else {
	gnames[x] = NULL;
    }
    return gnames[x];
}

/*
 * getGnameS() takes a group name, gets the gid, and creates an entry in the
 * table to hold a string containing the group name.
 */
char *getGnameS(char *gname)
{
    struct group *gr;
    int x;

    x = 0;
    while (x < gid_used) {
	if (!strcmp(gnames[x], gname)) {
	    return gnames[x];
	}
	x++;
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in getGname()\n");
	exit(RPMERR_INTERNAL);
    }
    
    gr = getgrnam(gname);
    gid_used++;
    if (gr) {
    	gids[x] = gr->gr_gid;
	gnames[x] = strdup(gr->gr_name);
    } else {
    	gids[x] = -1;
	gnames[x] = strdup(gname);
    }
    return gnames[x];
}

time_t *getBuildTime(void)
{
    static time_t buildTime = 0;

    if (! buildTime) {
	buildTime = time(NULL);
    }

    return &buildTime;
}

char *buildHost(void)
{
    static char hostname[1024];
    static int gotit = 0;
    struct hostent *hbn;

    if (! gotit) {
        gethostname(hostname, sizeof(hostname));
	if ((hbn = gethostbyname(hostname))) {
	    strcpy(hostname, hbn->h_name);
	} else {
	    rpmMessage(RPMMESS_WARNING, "Could not canonicalize hostname: %s\n",
		    hostname);
	}
	gotit = 1;
    }
    return(hostname);
}
