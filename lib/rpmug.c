#include "system.h"

#include <errno.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmmacro.h>

#include "lib/misc.h"
#include "lib/rpmug.h"
#include "debug.h"

static char *pwpath = NULL;
static char *grppath = NULL;

static const char *getpath(const char *bn, const char *dfl, char **dest)
{
    if (*dest == NULL) {
	char *s = rpmExpand("%{_", bn, "_path}", NULL);
	if (*s == '%' || *s == '\0') {
	    free(s);
	    s = xstrdup(dfl);
	}
	*dest = s;
    }
    return *dest;
}

static const char *pwfile(void)
{
    return getpath("passwd", "/etc/passwd", &pwpath);
}

static const char *grpfile(void)
{
    return getpath("group", "/etc/group", &grppath);
}

/*
 * Lookup an arbitrary field based on contents of another in a ':' delimited
 * file, such as /etc/passwd or /etc/group.
 */
static int lookup_field(const char *path, const char *val, int vcol, int rcol,
			char **ret)
{
    int rc = -1; /* assume not found */
    char *str, buf[BUFSIZ];
    FILE *f = fopen(path, "r");
    if (f == NULL) {
	rpmlog(RPMLOG_ERR, _("failed to open %s for id/name lookup: %s\n"),
		path, strerror(errno));
	return rc;
    }

    while ((str = fgets(buf, sizeof(buf), f)) != NULL) {
	int nf = vcol > rcol ? vcol : rcol;
	const char *fields[nf + 1];
	char *tok, *save = NULL;
	int col = -1;

	while ((tok = strtok_r(str, ":", &save)) != NULL) {
	    fields[++col] = tok;
	    str = NULL;
	    if (col >= nf)
		break;
	}

	if (col >= nf) {
	    if (rstreq(val, fields[vcol])) {
		*ret = xstrdup(fields[rcol]);
		rc = 0;
	    }
	}
    }

    fclose(f);

    return rc;
}

/* atol() with error handling, return 0/-1 on success/failure */
static int stol(const char *s, long *ret)
{
    int rc = 0;
    char *end = NULL;
    long val = strtol(s, &end, 10);

    /* only accept fully numeric data */
    if (*s == '\0' || *end != '\0')
	rc = -1;

    if ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)
	rc = -1;

    if (rc == 0)
	*ret = val;

    return rc;
}

static int lookup_num(const char *path, const char *val, int vcol, int rcol,
			long *ret)
{
    char *buf = NULL;
    int rc = lookup_field(path, val, vcol, rcol, &buf);
    if (rc == 0) {
	rc = stol(buf, ret);
	free(buf);
    }
    return rc;
}

static int lookup_str(const char *path, long val, int vcol, int rcol,
			char **ret)
{
    char *vbuf = NULL;
    rasprintf(&vbuf, "%ld", val);
    int rc = lookup_field(path, vbuf, vcol, rcol, ret);
    free(vbuf);
    return rc;
}

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
    static uid_t lastUid;

    if (!thisUname) {
	lastUname = rfree(lastUname);
	return -1;
    } else if (rstreq(thisUname, UID_0_USER)) {
	*uid = 0;
	return 0;
    }

    if (lastUname == NULL || !rstreq(thisUname, lastUname)) {
	long id;
	if (lookup_num(pwfile(), thisUname, 0, 2, &id))
	    return -1;

	free(lastUname);
	lastUname = xstrdup(thisUname);
	lastUid = id;
    }

    *uid = lastUid;

    return 0;
}

int rpmugGid(const char * thisGname, gid_t * gid)
{
    static char * lastGname = NULL;
    static gid_t lastGid;

    if (thisGname == NULL) {
	lastGname = rfree(lastGname);
	return -1;
    } else if (rstreq(thisGname, GID_0_GROUP)) {
	*gid = 0;
	return 0;
    }

    if (lastGname == NULL || !rstreq(thisGname, lastGname)) {
	long id;
	if (lookup_num(grpfile(), thisGname, 0, 2, &id))
	    return -1;
	free(lastGname);
	lastGname = xstrdup(thisGname);
	lastGid = id;
    }

    *gid = lastGid;

    return 0;
}

const char * rpmugUname(uid_t uid)
{
    static uid_t lastUid = (uid_t) -1;
    static char * lastUname = NULL;

    if (uid == (uid_t) -1) {
	lastUid = (uid_t) -1;
	lastUname = rfree(lastUname);
	return NULL;
    } else if (uid == (uid_t) 0) {
	return UID_0_USER;
    } else if (uid == lastUid) {
	return lastUname;
    } else {
	char *uname = NULL;

	if (lookup_str(pwfile(), uid, 2, 0, &uname))
	    return NULL;

	lastUid = uid;
	free(lastUname);
	lastUname = uname;

	return lastUname;
    }
}

const char * rpmugGname(gid_t gid)
{
    static gid_t lastGid = (gid_t) -1;
    static char * lastGname = NULL;

    if (gid == (gid_t) -1) {
	lastGid = (gid_t) -1;
	lastGname = rfree(lastGname);
	return NULL;
    } else if (gid == (gid_t) 0) {
	return GID_0_GROUP;
    } else if (gid == lastGid) {
	return lastGname;
    } else {
	char *gname = NULL;

	if (lookup_str(grpfile(), gid, 2, 0, &gname))
	    return NULL;

	lastGid = gid;
	free(lastGname);
	lastGname = gname;

	return lastGname;
    }
}

void rpmugFree(void)
{
    rpmugUid(NULL, NULL);
    rpmugGid(NULL, NULL);
    rpmugUname(-1);
    rpmugGname(-1);
    pwpath = rfree(pwpath);
    grppath = rfree(grppath);
}
