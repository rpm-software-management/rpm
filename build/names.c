/** \ingroup rpmbuild
 * \file build/names.c
 * Simple user/group name/id cache (plus hostname and buildtime)
 */


#include "system.h"

#include "rpmbuild.h"
#include "debug.h"

static uid_t uids[1024];
/*@owned@*/ /*@null@*/ static const char *unames[1024];
static int uid_used = 0;

static gid_t gids[1024];
/*@owned@*/ /*@null@*/ static const char *gnames[1024];
static int gid_used = 0;
    
/*@-nullderef@*/	/* FIX: shrug */
void freeNames(void)
{
    int x;
    for (x = 0; x < uid_used; x++)
	unames[x] = _free(unames[x]);
    for (x = 0; x < gid_used; x++)
	gnames[x] = _free(gnames[x]);
}

const char *getUname(uid_t uid)
{
    struct passwd *pw;
    int x;

    for (x = 0; x < uid_used; x++) {
	if (uids[x] == uid)
	    return unames[x];
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024)
	rpmlog(RPMLOG_CRIT, _("getUname: too many uid's\n"));
    uid_used++;
    
    pw = getpwuid(uid);
    uids[x] = uid;
    unames[x] = (pw ? xstrdup(pw->pw_name) : NULL);
    return unames[x];
}

const char *getUnameS(const char *uname)
{
    struct passwd *pw;
    int x;

    for (x = 0; x < uid_used; x++) {
	if (!strcmp(unames[x],uname))
	    return unames[x];
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024)
	rpmlog(RPMLOG_CRIT, _("getUnameS: too many uid's\n"));
    uid_used++;
    
    pw = getpwnam(uname);
    uids[x] = (pw ? pw->pw_uid : -1);
    unames[x] = (pw ? xstrdup(pw->pw_name) : xstrdup(uname));
    return unames[x];
}

const char *getGname(gid_t gid)
{
    struct group *gr;
    int x;

    for (x = 0; x < gid_used; x++) {
	if (gids[x] == gid)
	    return gnames[x];
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024)
	rpmlog(RPMLOG_CRIT, _("getGname: too many gid's\n"));
    gid_used++;
    
    gr = getgrgid(gid);
    gids[x] = gid;
    gnames[x] = (gr ? xstrdup(gr->gr_name) : NULL);
    return gnames[x];
}

const char *getGnameS(const char *gname)
{
    struct group *gr;
    int x;

    for (x = 0; x < gid_used; x++) {
	if (!strcmp(gnames[x], gname))
	    return gnames[x];
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024)
	rpmlog(RPMLOG_CRIT, _("getGnameS: too many gid's\n"));
    gid_used++;
    
    gr = getgrnam(gname);
    gids[x] = (gr ? gr->gr_gid : -1);
    gnames[x] = (gr ? xstrdup(gr->gr_name) : xstrdup(gname));
    return gnames[x];
}
/*@=nullderef@*/

int_32 *const getBuildTime(void)
{
    static int_32 buildTime[1];

    if (buildTime[0] == 0)
	buildTime[0] = time(NULL);
    return buildTime;
}

const char *const buildHost(void)
{
    static char hostname[1024];
    static int gotit = 0;
    struct hostent *hbn;

    if (! gotit) {
        (void) gethostname(hostname, sizeof(hostname));
	if ((hbn = /*@-unrecog@*/ gethostbyname(hostname) /*@=unrecog@*/ ))
	    strcpy(hostname, hbn->h_name);
	else
	    rpmMessage(RPMMESS_WARNING,
			_("Could not canonicalize hostname: %s\n"), hostname);
	gotit = 1;
    }
    return(hostname);
}
