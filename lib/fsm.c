/** \ingroup payload
 * \file lib/fsm.c
 * File state machine to handle a payload from a package.
 */

#include "system.h"

#include <utime.h>
#include <errno.h>
#if WITH_CAP
#include <sys/capability.h>
#endif

#include <rpm/rpmte.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmio_internal.h"	/* fdInit/FiniDigest */
#include "lib/fsm.h"
#include "lib/rpmte_internal.h"	/* XXX rpmfs */
#include "lib/rpmplugins.h"	/* rpm plugins hooks */
#include "lib/rpmug.h"

#include "debug.h"

#define	_FSM_DEBUG	0
int _fsm_debug = _FSM_DEBUG;

typedef struct fsm_s * FSM_t;

/* XXX Failure to remove is not (yet) cause for failure. */
static int strict_erasures = 0;

/** \ingroup payload
 * File name and stat information.
 */
struct fsm_s {
    char * path;		/*!< Current file name. */
    char * suffix;		/*!< Current file suffix. */
    int postpone;		/*!< Skip remaining stages? */
    int exists;			/*!< Does current file exist on disk? */

    rpmfi fi;			/*!< File iterator. */
};

#define	SUFFIX_RPMORIG	".rpmorig"
#define	SUFFIX_RPMSAVE	".rpmsave"
#define	SUFFIX_RPMNEW	".rpmnew"

/* Default directory and file permissions if not mapped */
#define _dirPerms 0755
#define _filePerms 0644

/* 
 * XXX Forward declarations for previously exported functions to avoid moving 
 * things around needlessly 
 */ 
static const char * fileActionString(rpmFileAction a);

/** \ingroup payload
 * Build path to file from file info, optionally ornamented with suffix.
 * @param fi		file info iterator
 * @param isDir		directory or regular path?
 * @param suffix	suffix to use (NULL disables)
 * @retval		path to file (malloced)
 */
static char * fsmFsPath(rpmfi fi, int isDir,
			const char * suffix)
{
    return rstrscat(NULL,
		    rpmfiDN(fi), rpmfiBN(fi),
		    (!isDir && suffix) ? suffix : "",
		    NULL);
}

/** \ingroup payload
 * Directory name iterator.
 */
typedef struct dnli_s {
    rpmfiles fi;
    char * active;
    int reverse;
    int isave;
    int i;
} * DNLI_t;

/** \ingroup payload
 * Destroy directory name iterator.
 * @param a		directory name iterator
 * @retval		NULL always
 */
static DNLI_t dnlFreeIterator(DNLI_t dnli)
{
    if (dnli) {
	if (dnli->active) free(dnli->active);
	free(dnli);
    }
    return NULL;
}

/** \ingroup payload
 * Create directory name iterator.
 * @param fi		file info set
 * @param fs		file state set
 * @param reverse	traverse directory names in reverse order?
 * @return		directory name iterator
 */
static DNLI_t dnlInitIterator(rpmfiles fi, rpmfs fs, int reverse)
{
    DNLI_t dnli;
    int i, j;
    int dc;

    if (fi == NULL)
	return NULL;
    dc = rpmfilesDC(fi);
    dnli = xcalloc(1, sizeof(*dnli));
    dnli->fi = fi;
    dnli->reverse = reverse;
    dnli->i = (reverse ? dc : 0);

    if (dc) {
	dnli->active = xcalloc(dc, sizeof(*dnli->active));
	int fc = rpmfilesFC(fi);

	/* Identify parent directories not skipped. */
	for (i = 0; i < fc; i++)
            if (!XFA_SKIPPING(rpmfsGetAction(fs, i)))
		dnli->active[rpmfilesDI(fi, i)] = 1;

	/* Exclude parent directories that are explicitly included. */
	for (i = 0; i < fc; i++) {
	    int dil;
	    size_t dnlen, bnlen;

	    if (!S_ISDIR(rpmfilesFMode(fi, i)))
		continue;

	    dil = rpmfilesDI(fi, i);
	    dnlen = strlen(rpmfilesDN(fi, dil));
	    bnlen = strlen(rpmfilesBN(fi, i));

	    for (j = 0; j < dc; j++) {
		const char * dnl;
		size_t jlen;

		if (!dnli->active[j] || j == dil)
		    continue;
		dnl = rpmfilesDN(fi, j);
		jlen = strlen(dnl);
		if (jlen != (dnlen+bnlen+1))
		    continue;
		if (!rstreqn(dnl, rpmfilesDN(fi, dil), dnlen))
		    continue;
		if (!rstreqn(dnl+dnlen, rpmfilesBN(fi, i), bnlen))
		    continue;
		if (dnl[dnlen+bnlen] != '/' || dnl[dnlen+bnlen+1] != '\0')
		    continue;
		/* This directory is included in the package. */
		dnli->active[j] = 0;
		break;
	    }
	}

	/* Print only once per package. */
	if (!reverse) {
	    j = 0;
	    for (i = 0; i < dc; i++) {
		if (!dnli->active[i]) continue;
		if (j == 0) {
		    j = 1;
		    rpmlog(RPMLOG_DEBUG,
	"========== Directories not explicitly included in package:\n");
		}
		rpmlog(RPMLOG_DEBUG, "%10d %s\n", i, rpmfilesDN(fi, i));
	    }
	    if (j)
		rpmlog(RPMLOG_DEBUG, "==========\n");
	}
    }
    return dnli;
}

/** \ingroup payload
 * Return next directory name (from file info).
 * @param dnli		directory name iterator
 * @return		next directory name
 */
static
const char * dnlNextIterator(DNLI_t dnli)
{
    const char * dn = NULL;

    if (dnli) {
	rpmfiles fi = dnli->fi;
	int dc = rpmfilesDC(fi);
	int i = -1;

	if (dnli->active)
	do {
	    i = (!dnli->reverse ? dnli->i++ : --dnli->i);
	} while (i >= 0 && i < dc && !dnli->active[i]);

	if (i >= 0 && i < dc)
	    dn = rpmfilesDN(fi, i);
	else
	    i = -1;
	dnli->isave = i;
    }
    return dn;
}

static FSM_t fsmNew(rpmts ts, rpmte te, rpmfi fi)
{
    FSM_t fsm = xcalloc(1, sizeof(*fsm));

    fsm->fi = fi;

    return fsm;
}

static FSM_t fsmFree(FSM_t fsm)
{
    fsm->path = _free(fsm->path);
    fsm->suffix = _free(fsm->suffix);

    free(fsm);
    return NULL;
}

static int fsmSetFCaps(const char *path, const char *captxt)
{
    int rc = 0;
#if WITH_CAP
    if (captxt && *captxt != '\0') {
	cap_t fcaps = cap_from_text(captxt);
	if (fcaps == NULL || cap_set_file(path, fcaps) != 0) {
	    rc = RPMERR_SETCAP_FAILED;
	}
	cap_free(fcaps);
    } 
#endif
    return rc;
}

/**
 * Map file stat(2) info.
 * @param fi		file info iterator
 * @param warn		warn on user/group mapping error
 * @retval st		mapped stat(2) data
 * @return		0 on success
 */
static int fsmMapAttrs(rpmfi fi, int warn, struct stat * st)
{
    mode_t finalMode = rpmfiFMode(fi);
    const char *user = rpmfiFUser(fi);
    const char *group = rpmfiFGroup(fi);
    uid_t uid = 0;
    gid_t gid = 0;

    if (user && rpmugUid(user, &uid)) {
	if (warn)
	    rpmlog(RPMLOG_WARNING,
		    _("user %s does not exist - using root\n"), user);
	finalMode &= ~S_ISUID;	  /* turn off suid bit */
    }

    if (group && rpmugGid(group, &gid)) {
	if (warn)
	    rpmlog(RPMLOG_WARNING,
		    _("group %s does not exist - using root\n"), group);
	finalMode &= ~S_ISGID;	/* turn off sgid bit */
    }

    st->st_uid = uid;
    st->st_gid = gid;

    st->st_mode = finalMode;
    st->st_dev = 0;
    st->st_ino = rpmfiFInode(fi);
    st->st_rdev = rpmfiFRdev(fi);
    st->st_mtime = rpmfiFMtime(fi);
    st->st_size = rpmfiFSize(fi);
    st->st_nlink = rpmfiFNlink(fi);

    if ((S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
	    && st->st_nlink == 0) {
	st->st_nlink = 1;
    }
    return 0;
}

/** \ingroup payload
 * Create file from payload stream.
 * @return		0 on success
 */
static int expandRegular(rpmfi fi, const char *dest, rpmpsm psm, int nodigest, int nocontent)
{
    FD_t wfd = NULL;
    int rc = 0;

    wfd = Fopen(dest, "w.ufdio");
    if (Ferror(wfd)) {
	rc = RPMERR_OPEN_FAILED;
	goto exit;
    }

    if (!nocontent)
	rc = rpmfiArchiveReadToFile(fi, wfd, nodigest);
exit:
    if (wfd) {
	int myerrno = errno;
	Fclose(wfd);
	errno = myerrno;
    }
    return rc;
}

static int fsmMkfile(rpmfi fi, const char *dest, rpmfiles files,
		     rpmpsm psm, int nodigest, int *setmeta,
		     int * firsthardlink)
{
    int rc = 0;
    int numHardlinks = rpmfiFNlink(fi);

    if (numHardlinks > 1) {
	/* Create first hardlinked file empty */
	if (*firsthardlink < 0) {
	    *firsthardlink = rpmfiFX(fi);
	    rc = expandRegular(fi, dest, psm, nodigest, 1);
	} else {
	    /* Create hard links for others */
	    rc = link(rpmfilesFN(files, *firsthardlink), dest);
	    if (rc < 0) {
		rc = RPMERR_LINK_FAILED;
	    }
	}
    }
    /* Write normal files or fill the last hardlinked (already
       existing) file with content */
    if (numHardlinks<=1) {
	if (!rc)
	    rc = expandRegular(fi, dest, psm, nodigest, 0);
    } else if (rpmfiArchiveHasContent(fi)) {
	if (!rc)
	    rc = expandRegular(fi, dest, psm, nodigest, 0);
	*firsthardlink = -1;
    } else {
	*setmeta = 0;
    }

    return rc;
}

static int fsmReadLink(const char *path,
		       char *buf, size_t bufsize, size_t *linklen)
{
    ssize_t llen = readlink(path, buf, bufsize - 1);
    int rc = RPMERR_READLINK_FAILED;

    if (_fsm_debug) {
        rpmlog(RPMLOG_DEBUG, " %8s (%s, buf, %d) %s\n",
	       __func__,
               path, (int)(bufsize -1), (llen < 0 ? strerror(errno) : ""));
    }

    if (llen >= 0) {
	buf[llen] = '\0';
	rc = 0;
	*linklen = llen;
    }
    return rc;
}

static int fsmStat(const char *path, int dolstat, struct stat *sb)
{
    int rc;
    if (dolstat){
	rc = lstat(path, sb);
    } else {
        rc = stat(path, sb);
    }
    if (_fsm_debug && rc && errno != ENOENT)
        rpmlog(RPMLOG_DEBUG, " %8s (%s, ost) %s\n",
               __func__,
               path, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0) {
        rc = (errno == ENOENT ? RPMERR_ENOENT : RPMERR_LSTAT_FAILED);
	/* WTH is this, and is it really needed, still? */
        memset(sb, 0, sizeof(*sb));	/* XXX s390x hackery */
    }
    return rc;
}

static int fsmRmdir(const char *path)
{
    int rc = rmdir(path);
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s) %s\n", __func__,
	       path, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)
	switch (errno) {
	case ENOENT:        rc = RPMERR_ENOENT;    break;
	case ENOTEMPTY:     rc = RPMERR_ENOTEMPTY; break;
	default:            rc = RPMERR_RMDIR_FAILED; break;
	}
    return rc;
}

static int fsmMkdir(const char *path, mode_t mode)
{
    int rc = mkdir(path, (mode & 07777));
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%04o) %s\n", __func__,
	       path, (unsigned)(mode & 07777),
	       (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = RPMERR_MKDIR_FAILED;
    return rc;
}

static int fsmMkfifo(const char *path, mode_t mode)
{
    int rc = mkfifo(path, (mode & 07777));

    if (_fsm_debug) {
	rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%04o) %s\n",
	       __func__, path, (unsigned)(mode & 07777),
	       (rc < 0 ? strerror(errno) : ""));
    }

    if (rc < 0)
	rc = RPMERR_MKFIFO_FAILED;

    return rc;
}

static int fsmMknod(const char *path, mode_t mode, dev_t dev)
{
    /* FIX: check S_IFIFO or dev != 0 */
    int rc = mknod(path, (mode & ~07777), dev);

    if (_fsm_debug) {
	rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%o, 0x%x) %s\n",
	       __func__, path, (unsigned)(mode & ~07777),
	       (unsigned)dev, (rc < 0 ? strerror(errno) : ""));
    }

    if (rc < 0)
	rc = RPMERR_MKNOD_FAILED;

    return rc;
}

/**
 * Create (if necessary) directories not explicitly included in package.
 * @param dnli		file state machine data
 * @param plugins	rpm plugins handle
 * @return		0 on success
 */
static int fsmMkdirs(rpmfiles files, rpmfs fs, rpmPlugins plugins)
{
    DNLI_t dnli = dnlInitIterator(files, fs, 0);
    struct stat sb;
    const char *dpath;
    int dc = rpmfilesDC(files);
    int rc = 0;
    int i;
    int ldnlen = 0;
    int ldnalloc = 0;
    char * ldn = NULL;
    short * dnlx = NULL; 

    dnlx = (dc ? xcalloc(dc, sizeof(*dnlx)) : NULL);

    if (dnlx != NULL)
    while ((dpath = dnlNextIterator(dnli)) != NULL) {
	size_t dnlen = strlen(dpath);
	char * te, dn[dnlen+1];

	dc = dnli->isave;
	if (dc < 0) continue;
	dnlx[dc] = dnlen;
	if (dnlen <= 1)
	    continue;

	if (dnlen <= ldnlen && rstreq(dpath, ldn))
	    continue;

	/* Copy as we need to modify the string */
	(void) stpcpy(dn, dpath);

	/* Assume '/' directory exists, "mkdir -p" for others if non-existent */
	for (i = 1, te = dn + 1; *te != '\0'; te++, i++) {
	    if (*te != '/')
		continue;

	    *te = '\0';

	    /* Already validated? */
	    if (i < ldnlen &&
		(ldn[i] == '/' || ldn[i] == '\0') && rstreqn(dn, ldn, i))
	    {
		*te = '/';
		/* Move pre-existing path marker forward. */
		dnlx[dc] = (te - dn);
		continue;
	    }

	    /* Validate next component of path. */
	    rc = fsmStat(dn, 1, &sb); /* lstat */
	    *te = '/';

	    /* Directory already exists? */
	    if (rc == 0 && S_ISDIR(sb.st_mode)) {
		/* Move pre-existing path marker forward. */
		dnlx[dc] = (te - dn);
	    } else if (rc == RPMERR_ENOENT) {
		*te = '\0';
		mode_t mode = S_IFDIR | (_dirPerms & 07777);
		rpmFsmOp op = (FA_CREATE|FAF_UNOWNED);

		/* Run fsm file pre hook for all plugins */
		rc = rpmpluginsCallFsmFilePre(plugins, NULL, dn, mode, op);

		if (!rc)
		    rc = fsmMkdir(dn, mode);

		if (!rc) {
		    rc = rpmpluginsCallFsmFilePrepare(plugins, NULL, dn, dn,
						      mode, op);
		}

		/* Run fsm file post hook for all plugins */
		rpmpluginsCallFsmFilePost(plugins, NULL, dn, mode, op, rc);

		if (!rc) {
		    rpmlog(RPMLOG_DEBUG,
			    "%s directory created with perms %04o\n",
			    dn, (unsigned)(mode & 07777));
		}
		*te = '/';
	    }
	    if (rc)
		break;
	}
	if (rc) break;

	/* Save last validated path. */
	if (ldnalloc < (dnlen + 1)) {
	    ldnalloc = dnlen + 100;
	    ldn = xrealloc(ldn, ldnalloc);
	}
	if (ldn != NULL) { /* XXX can't happen */
	    strcpy(ldn, dn);
	    ldnlen = dnlen;
	}
    }
    free(dnlx);
    free(ldn);
    dnlFreeIterator(dnli);

    return rc;
}

static void removeSBITS(const char *path)
{
    struct stat stb;
    if (lstat(path, &stb) == 0 && S_ISREG(stb.st_mode)) {
	if ((stb.st_mode & 06000) != 0) {
	    (void) chmod(path, stb.st_mode & 0777);
	}
#if WITH_CAP
	if (stb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
	    (void) cap_set_file(path, NULL);
	}
#endif
    }
}

static int fsmInit(FSM_t fsm, rpmFileAction action, rpmElementType goal, struct stat *st)
{
    int rc = 0;

    memset(st, 0, sizeof(*st));
    fsm->path = _free(fsm->path);
    fsm->postpone = 0;
    fsm->exists = 0;

    /* Generate file path. */
    fsm->path = fsmFsPath(fsm->fi, S_ISDIR(rpmfiFMode(fsm->fi)), fsm->suffix);

    /* Perform lstat/stat for disk file. */
    if (fsm->path != NULL &&
	!(goal == TR_ADDED && S_ISREG(rpmfiFMode(fsm->fi))))
    {
	rc = fsmStat(fsm->path, 1, st);
	if (rc == RPMERR_ENOENT) {
	    // errno = saveerrno; XXX temporary commented out
	    rc = 0;
	    fsm->exists = 0;
	} else if (rc == 0) {
	    fsm->exists = 1;
	}
    } else {
	/* Regular files are created with tmp suffix, assume they dont exist */
	fsm->exists = 0;
    }
    if (rc) return rc;

    fsm->postpone = XFA_SKIPPING(action);

    rpmlog(RPMLOG_DEBUG, "%-10s %06o%3d (%4d,%4d)%6d %s\n",
	   fileActionString(action), (int)st->st_mode,
	   (int)st->st_nlink, (int)st->st_uid,
	   (int)st->st_gid, (int)st->st_size,
	    (fsm->path ? fsm->path : ""));

    return rc;

}

static int fsmSymlink(const char *opath, const char *path)
{
    int rc = symlink(opath, path);

    if (_fsm_debug) {
	rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", __func__,
	       opath, path, (rc < 0 ? strerror(errno) : ""));
    }

    if (rc < 0)
	rc = RPMERR_SYMLINK_FAILED;
    return rc;
}

static int fsmUnlink(const char *path)
{
    int rc = 0;
    removeSBITS(path);
    rc = unlink(path);
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s) %s\n", __func__,
	       path, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)
	rc = (errno == ENOENT ? RPMERR_ENOENT : RPMERR_UNLINK_FAILED);
    return rc;
}

static int fsmRename(const char *opath, const char *path)
{
    removeSBITS(path);
    int rc = rename(opath, path);
#if defined(ETXTBSY) && defined(__HPUX__)
    /* XXX HP-UX (and other os'es) don't permit rename to busy files. */
    if (rc && errno == ETXTBSY) {
	char *rmpath = NULL;
	rstrscat(&rmpath, path, "-RPMDELETE", NULL);
	rc = rename(path, rmpath);
	if (!rc) rc = rename(opath, path);
	free(rmpath);
    }
#endif
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", __func__,
	       opath, path, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = RPMERR_RENAME_FAILED;
    return rc;
}


static int fsmChown(const char *path, mode_t mode, uid_t uid, gid_t gid)
{
    int rc = S_ISLNK(mode) ? lchown(path, uid, gid) : chown(path, uid, gid);
    if (rc < 0) {
	struct stat st;
	if (lstat(path, &st) == 0 && st.st_uid == uid && st.st_gid == gid)
	    rc = 0;
    }
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, %d) %s\n", __func__,
	       path, (int)uid, (int)gid,
	       (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = RPMERR_CHOWN_FAILED;
    return rc;
}

static int fsmChmod(const char *path, mode_t mode)
{
    int rc = chmod(path, (mode & 07777));
    if (rc < 0) {
	struct stat st;
	if (lstat(path, &st) == 0 && (st.st_mode & 07777) == (mode & 07777))
	    rc = 0;
    }
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%04o) %s\n", __func__,
	       path, (unsigned)(mode & 07777),
	       (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = RPMERR_CHMOD_FAILED;
    return rc;
}

static int fsmUtime(const char *path, mode_t mode, time_t mtime)
{
    int rc = 0;
    struct timeval stamps[2] = {
	{ .tv_sec = mtime, .tv_usec = 0 },
	{ .tv_sec = mtime, .tv_usec = 0 },
    };

#if HAVE_LUTIMES
    rc = lutimes(path, stamps);
#else
    if (!S_ISLNK(mode))
	rc = utimes(path, stamps);
#endif
    
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, 0x%x) %s\n", __func__,
	       path, (unsigned)mtime, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = RPMERR_UTIME_FAILED;
    /* ...but utime error is not critical for directories */
    if (rc && S_ISDIR(mode))
	rc = 0;
    return rc;
}

static int fsmVerify(FSM_t fsm, const struct stat *st, struct stat *ost)
{
    int rc;
    int saveerrno = errno;

    if (!fsm->exists) {
        return RPMERR_ENOENT;
    }
    if (S_ISREG(st->st_mode)) {
	/* HP-UX (and other os'es) don't permit unlink on busy files. */
	char *rmpath = rstrscat(NULL, fsm->path, "-RPMDELETE", NULL);
	rc = fsmRename(fsm->path, rmpath);
	/* XXX shouldn't we take unlink return code here? */
	if (!rc)
	    (void) fsmUnlink(rmpath);
	else
	    rc = RPMERR_UNLINK_FAILED;
	free(rmpath);
        return (rc ? rc : RPMERR_ENOENT);	/* XXX HACK */
    } else if (S_ISDIR(st->st_mode)) {
        if (S_ISDIR(ost->st_mode)) return 0;
        if (S_ISLNK(ost->st_mode)) {
            rc = fsmStat(fsm->path, 0, ost);
            if (rc == RPMERR_ENOENT) rc = 0;
            if (rc) return rc;
            errno = saveerrno;
            if (S_ISDIR(ost->st_mode)) return 0;
        }
    } else if (S_ISLNK(st->st_mode)) {
        if (S_ISLNK(ost->st_mode)) {
            char buf[8 * BUFSIZ];
            size_t len;
            rc = fsmReadLink(fsm->path, buf, 8 * BUFSIZ, &len);
            errno = saveerrno;
            if (rc) return rc;
            if (rstreq(rpmfiFLink(fsm->fi), buf)) return 0;
        }
    } else if (S_ISFIFO(st->st_mode)) {
        if (S_ISFIFO(ost->st_mode)) return 0;
    } else if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
        if ((S_ISCHR(ost->st_mode) || S_ISBLK(ost->st_mode)) &&
            (ost->st_rdev == st->st_rdev)) return 0;
    } else if (S_ISSOCK(st->st_mode)) {
        if (S_ISSOCK(ost->st_mode)) return 0;
    }
    /* XXX shouldn't do this with commit/undo. */
    rc = fsmUnlink(fsm->path);
    if (rc == 0)	rc = RPMERR_ENOENT;
    return (rc ? rc : RPMERR_ENOENT);	/* XXX HACK */
}

#define	IS_DEV_LOG(_x)	\
	((_x) != NULL && strlen(_x) >= (sizeof("/dev/log")-1) && \
	rstreqn((_x), "/dev/log", sizeof("/dev/log")-1) && \
	((_x)[sizeof("/dev/log")-1] == '\0' || \
	 (_x)[sizeof("/dev/log")-1] == ';'))



/* Rename pre-existing modified or unmanaged file. */
static int fsmBackup(FSM_t fsm, rpmFileAction action, mode_t mode)
{
    int rc = 0;
    const char *suffix = NULL;

    if (!(rpmfiFFlags(fsm->fi) & RPMFILE_GHOST)) {
	switch (action) {
	case FA_SAVE:
	    suffix = SUFFIX_RPMSAVE;
	    break;
	case FA_BACKUP:
	    suffix = SUFFIX_RPMORIG;
	    break;
	default:
	    break;
	}
    }

    if (suffix) {
	char * opath = fsmFsPath(fsm->fi, S_ISDIR(mode), NULL);
	char * path = fsmFsPath(fsm->fi, 0, suffix);
	rc = fsmRename(opath, path);
	if (!rc) {
	    rpmlog(RPMLOG_WARNING, _("%s saved as %s\n"), opath, path);
	    fsm->exists = 0; /* it doesn't exist anymore... */
	}
	free(path);
	free(opath);
    }
    return rc;
}

static int fsmSetmeta(FSM_t fsm, rpmPlugins plugins,
		      rpmFileAction action, const struct stat * st)
{
    int rc = 0;
    const char *dest = rpmfiFN(fsm->fi);

    if (!rc && !getuid()) {
	rc = fsmChown(fsm->path, st->st_mode, st->st_uid, st->st_gid);
    }
    if (!rc && !S_ISLNK(st->st_mode)) {
	rc = fsmChmod(fsm->path, st->st_mode);
    }
    /* Set file capabilities (if enabled) */
    if (!rc && S_ISREG(st->st_mode) && !getuid()) {
	rc = fsmSetFCaps(fsm->path, rpmfiFCaps(fsm->fi));
    }
    if (!rc) {
	rc = fsmUtime(fsm->path, st->st_mode, rpmfiFMtime(fsm->fi));
    }
    if (!rc) {
	rc = rpmpluginsCallFsmFilePrepare(plugins, fsm->fi,
					  fsm->path, dest, st->st_mode, action);
    }

    return rc;
}

static int fsmCommit(FSM_t fsm, rpmFileAction action, const struct stat * st)
{
    int rc = 0;

    /* XXX Special case /dev/log, which shouldn't be packaged anyways */
    if (S_ISSOCK(st->st_mode) && IS_DEV_LOG(fsm->path))
	return 0;

    /* Backup on-disk file if needed. Directories are handled earlier */
    if (!S_ISDIR(st->st_mode))
	rc = fsmBackup(fsm, action, st->st_mode);

    if (!rc) {
	const char *nsuffix = (action == FA_ALTNAME) ? SUFFIX_RPMNEW : NULL;
	char *dest = fsm->path;
	/* Construct final destination path (nsuffix is usually NULL) */
	if (!S_ISDIR(st->st_mode) && fsm->suffix)
	    dest = fsmFsPath(fsm->fi, 0, nsuffix);

	/* Rename temporary to final file name if needed. */
	if (dest != fsm->path) {
	    rc = fsmRename(fsm->path, dest);
	    if (!rc && nsuffix) {
		char * opath = fsmFsPath(fsm->fi, 0, NULL);
		rpmlog(RPMLOG_WARNING, _("%s created as %s\n"),
		       opath, dest);
		free(opath);
	    }
	    free(fsm->path);
	    fsm->path = dest;
	}
    }

    return rc;
}

/**
 * Return formatted string representation of file disposition.
 * @param a		file disposition
 * @return		formatted string
 */
static const char * fileActionString(rpmFileAction a)
{
    switch (a) {
    case FA_UNKNOWN:	return "unknown";
    case FA_CREATE:	return "create";
    case FA_BACKUP:	return "backup";
    case FA_SAVE:	return "save";
    case FA_SKIP:	return "skip";
    case FA_ALTNAME:	return "altname";
    case FA_ERASE:	return "erase";
    case FA_SKIPNSTATE: return "skipnstate";
    case FA_SKIPNETSHARED: return "skipnetshared";
    case FA_SKIPCOLOR:	return "skipcolor";
    default:		return "???";
    }
}

/* Remember any non-regular file state for recording in the rpmdb */
static void setFileState(rpmfs fs, int i)
{
    switch (rpmfsGetAction(fs, i)) {
    case FA_SKIPNSTATE:
	rpmfsSetState(fs, i, RPMFILE_STATE_NOTINSTALLED);
	break;
    case FA_SKIPNETSHARED:
	rpmfsSetState(fs, i, RPMFILE_STATE_NETSHARED);
	break;
    case FA_SKIPCOLOR:
	rpmfsSetState(fs, i, RPMFILE_STATE_WRONGCOLOR);
	break;
    default:
	break;
    }
}

int rpmPackageFilesInstall(rpmts ts, rpmte te, rpmfiles files, FD_t cfd,
              rpmpsm psm, char ** failedFile)
{
    rpmfi fi = rpmfiNewArchiveReader(cfd, files, RPMFI_ITER_READ_ARCHIVE);
    rpmfs fs = rpmteGetFileStates(te);
    FSM_t fsm = fsmNew(ts, te, fi);
    rpmPlugins plugins = rpmtsPlugins(ts);
    struct stat sb;
    struct stat osb;
    int saveerrno = errno;
    int rc = 0;
    int nodigest = (rpmtsFlags(ts) & RPMTRANS_FLAG_NOFILEDIGEST) ? 1 : 0;
    int firsthardlink = -1;
    rpmFileAction action;

    /* transaction id used for temporary path suffix while installing */
    rasprintf(&fsm->suffix, ";%08x", (unsigned)rpmtsGetTid(ts));

    /* Detect and create directories not explicitly in package. */
    if (!rc) {
	rc = fsmMkdirs(files, fs, plugins);
    }

    while (!rc) {
	/* Read next payload header. */
	rc = rpmfiNext(fi);

	if (rc < 0) {
	    if (rc == RPMERR_ITER_END)
		rc = 0;
	    break;
	}

	action = rpmfsGetAction(fs, rpmfiFX(fsm->fi));

	/* Sets fsm->postpone for skipped files */
	rc = fsmInit(fsm, action, TR_ADDED, &osb);

	/* Remap file perms, owner, and group. */
	if (!rc)
	    rc = fsmMapAttrs(fsm->fi, 1, &sb);

        /* Exit on error. */
        if (rc)
            break;

	/* Run fsm file pre hook for all plugins */
	rc = rpmpluginsCallFsmFilePre(plugins, fsm->fi, fsm->path,
				      sb.st_mode, action);
	if (rc) {
	    fsm->postpone = 1;
	} else {
	    setFileState(fs, rpmfiFX(fi));
	}

        if (!fsm->postpone) {
	    int setmeta = 1;
            if (S_ISREG(sb.st_mode)) {
                rc = fsmVerify(fsm, &sb, &osb);
		if (rc == RPMERR_ENOENT) {
		    rc = fsmMkfile(fi, fsm->path, files, psm, nodigest,
				   &setmeta, &firsthardlink);
		}
            } else if (S_ISDIR(sb.st_mode)) {
		/* Directories replacing something need early backup */
                rc = fsmBackup(fsm, action, sb.st_mode);
                rc = fsmVerify(fsm, &sb, &osb);
                if (rc == RPMERR_ENOENT) {
                    mode_t mode = sb.st_mode;
                    mode &= ~07777;
                    mode |=  00700;
                    rc = fsmMkdir(fsm->path, mode);
                }
            } else if (S_ISLNK(sb.st_mode)) {
		/* Sane symlinks should fit in stack but to be safe... */
		char *buf = xmalloc(sb.st_size + 1);
		if (rpmfiArchiveRead(fi, buf, sb.st_size) != sb.st_size) {
                    rc = RPMERR_READ_FAILED;
                } else {
		    buf[sb.st_size] = '\0';
		    if (nodigest || rstreq(rpmfiFLink(fi), buf)) {
			rc = fsmVerify(fsm, &sb, &osb);
		    } else {
			rc = RPMERR_DIGEST_MISMATCH;
		    }
                    if (rc == RPMERR_ENOENT) {
                        rc = fsmSymlink(buf, fsm->path);
                    }
                }
		free(buf);
            } else if (S_ISFIFO(sb.st_mode)) {
                /* This mimics cpio S_ISSOCK() behavior but probably isn't right */
                rc = fsmVerify(fsm, &sb, &osb);
                if (rc == RPMERR_ENOENT) {
                    rc = fsmMkfifo(fsm->path, 0000);
                }
            } else if (S_ISCHR(sb.st_mode) ||
                       S_ISBLK(sb.st_mode) ||
                       S_ISSOCK(sb.st_mode))
            {
                rc = fsmVerify(fsm, &sb, &osb);
                if (rc == RPMERR_ENOENT) {
                    rc = fsmMknod(fsm->path, sb.st_mode, sb.st_rdev);
                }
            } else {
                /* XXX Special case /dev/log, which shouldn't be packaged anyways */
                if (!IS_DEV_LOG(fsm->path))
                    rc = RPMERR_UNKNOWN_FILETYPE;
            }
	    /* Set permissions, timestamps etc for non-hardlink entries */
	    if (!rc && setmeta) {
		rc = fsmSetmeta(fsm, plugins, action, &sb);
	    }
        } else if (firsthardlink >= 0 && rpmfiArchiveHasContent(fi)) {
	    /* we skip the hard linked file containing the content */
	    /* write the content to the first used instead */
	    rc = expandRegular(fi, rpmfilesFN(files, firsthardlink),
			       psm, nodigest, 0);
	    firsthardlink = -1;
	}

        if (rc) {
            if (!fsm->postpone) {
                /* XXX only erase if temp fn w suffix is in use */
                if (fsm->suffix) {
                    if (S_ISDIR(sb.st_mode)) {
                        (void) fsmRmdir(fsm->path);
                    } else {
                        (void) fsmUnlink(fsm->path);
                    }
                }
                errno = saveerrno;
            }
        } else {
	    /* Notify on success. */
	    rpmpsmNotify(psm, RPMCALLBACK_INST_PROGRESS, rpmfiArchiveTell(fi));

	    if (!fsm->postpone) {
                rc = fsmCommit(fsm, action, &sb);
	    }
	}

	if (rc)
	    *failedFile = xstrdup(fsm->path);

	/* Run fsm file post hook for all plugins */
	rpmpluginsCallFsmFilePost(plugins, fsm->fi, fsm->path,
				  sb.st_mode, action, rc);

    }

    /* No need to bother with close errors on read */
    rpmfiArchiveClose(fi);
    fsmFree(fsm);
    rpmfiFree(fi);

    return rc;
}


int rpmPackageFilesRemove(rpmts ts, rpmte te, rpmfiles files,
              rpmpsm psm, char ** failedFile)
{
    rpmfi fi = rpmfilesIter(files, RPMFI_ITER_BACK);
    rpmfs fs = rpmteGetFileStates(te);
    FSM_t fsm = fsmNew(ts, te, fi);
    rpmPlugins plugins = rpmtsPlugins(ts);
    struct stat sb;
    int rc = 0;

    while (!rc && rpmfiNext(fsm->fi) >= 0) {
	rpmFileAction action = rpmfsGetAction(fs, rpmfiFX(fsm->fi));
	rc = fsmInit(fsm, action, TR_REMOVED, &sb);

	/* Run fsm file pre hook for all plugins */
	rc = rpmpluginsCallFsmFilePre(plugins, fsm->fi, fsm->path,
				      sb.st_mode, action);

	if (!fsm->postpone)
	    rc = fsmBackup(fsm, action, sb.st_mode);

        /* Remove erased files. */
        if (!fsm->postpone && action == FA_ERASE) {
	    int missingok = (rpmfiFFlags(fi) & (RPMFILE_MISSINGOK | RPMFILE_GHOST));

            if (S_ISDIR(sb.st_mode)) {
                rc = fsmRmdir(fsm->path);
            } else {
                rc = fsmUnlink(fsm->path);
	    }

	    /*
	     * Missing %ghost or %missingok entries are not errors.
	     * XXX: Are non-existent files ever an actual error here? Afterall
	     * that's exactly what we're trying to accomplish here,
	     * and complaining about job already done seems like kinderkarten
	     * level "But it was MY turn!" whining...
	     */
	    if (rc == RPMERR_ENOENT && missingok) {
		rc = 0;
	    }

	    /*
	     * Dont whine on non-empty directories for now. We might be able
	     * to track at least some of the expected failures though,
	     * such as when we knowingly left config file backups etc behind.
	     */
	    if (rc == RPMERR_ENOTEMPTY) {
		rc = 0;
	    }

	    if (rc) {
		int lvl = strict_erasures ? RPMLOG_ERR : RPMLOG_WARNING;
		rpmlog(lvl, _("%s %s: remove failed: %s\n"),
			S_ISDIR(sb.st_mode) ? _("directory") : _("file"),
			fsm->path, strerror(errno));
            }
        }

	/* Run fsm file post hook for all plugins */
	rpmpluginsCallFsmFilePost(plugins, fsm->fi, fsm->path,
				  sb.st_mode, action, rc);

        /* XXX Failure to remove is not (yet) cause for failure. */
        if (!strict_erasures) rc = 0;

	if (rc)
	    *failedFile = xstrdup(fsm->path);

	if (rc == 0) {
	    /* Notify on success. */
	    /* On erase we're iterating backwards, fixup for progress */
	    rpm_loff_t amount = rpmfiFC(fsm->fi) - rpmfiFX(fsm->fi);
	    rpmpsmNotify(psm, RPMCALLBACK_UNINST_PROGRESS, amount);
	}
    }

    fsmFree(fsm);
    rpmfiFree(fi);

    return rc;
}


