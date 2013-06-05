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
#include <rpm/rpmsq.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmio_internal.h"	/* fdInit/FiniDigest */
#include "lib/cpio.h"
#include "lib/fsm.h"
#define	fsmUNSAFE	fsmStage
#include "lib/rpmfi_internal.h"	/* XXX fi->apath, ... */
#include "lib/rpmte_internal.h"	/* XXX rpmfs */
#include "lib/rpmts_internal.h"	/* rpmtsSELabelFoo() only */
#include "lib/rpmug.h"
#include "lib/cpio.h"

#include "debug.h"

#define	_FSM_DEBUG	0
int _fsm_debug = _FSM_DEBUG;

extern int _fsm_debug;

/** \ingroup payload
 */
enum cpioMapFlags_e {
    CPIO_MAP_PATH	= (1 << 0),
    CPIO_MAP_MODE	= (1 << 1),
    CPIO_MAP_UID	= (1 << 2),
    CPIO_MAP_GID	= (1 << 3),
    CPIO_FOLLOW_SYMLINKS= (1 << 4), /*!< only for building. */
    CPIO_MAP_ABSOLUTE	= (1 << 5),
    CPIO_MAP_ADDDOT	= (1 << 6),
    CPIO_MAP_TYPE	= (1 << 8),  /*!< only for building. */
    CPIO_SBIT_CHECK	= (1 << 9)
};
typedef rpmFlags cpioMapFlags;

typedef struct fsmIterator_s * FSMI_t;
typedef struct fsm_s * FSM_t;

typedef struct hardLink_s * hardLink_t;

typedef enum fileStage_e {
    FSM_PKGINSTALL,
    FSM_PKGERASE,
    FSM_PKGBUILD,
} fileStage;

/* XXX Failure to remove is not (yet) cause for failure. */
static int strict_erasures = 0;

/** \ingroup payload
 * Keeps track of the set of all hard links to a file in an archive.
 */
struct hardLink_s {
    hardLink_t next;
    const char ** nsuffix;
    int * filex;
    struct stat sb;
    nlink_t nlink;
    nlink_t linksLeft;
    int linkIndex;
    int createdPath;
};

/** \ingroup payload
 * Iterator across package file info, forward on install, backward on erase.
 */
struct fsmIterator_s {
    rpmfs fs;			/*!< file state info. */
    rpmfi fi;			/*!< transaction element file info. */
    int reverse;		/*!< reversed traversal? */
    int isave;			/*!< last returned iterator index. */
    int i;			/*!< iterator index. */
};

/** \ingroup payload
 * File name and stat information.
 */
struct fsm_s {
    char * path;		/*!< Current file name. */
    char * buf;			/*!<  read: Buffer. */
    size_t bufsize;		/*!<  read: Buffer allocated size. */
    FSMI_t iter;		/*!< File iterator. */
    int ix;			/*!< Current file iterator index. */
    hardLink_t links;		/*!< Pending hard linked file(s). */
    char ** failedFile;		/*!< First file name that failed. */
    const char * osuffix;	/*!< Old, preserved, file suffix. */
    const char * nsuffix;	/*!< New, created, file suffix. */
    char * suffix;		/*!< Current file suffix. */
    int postpone;		/*!< Skip remaining stages? */
    int diskchecked;		/*!< Has stat(2) been performed? */
    int exists;			/*!< Does current file exist on disk? */
    cpioMapFlags mapFlags;	/*!< Bit(s) to control mapping. */
    const char * dirName;	/*!< File directory name. */
    const char * baseName;	/*!< File base name. */
    struct selabel_handle *sehandle;	/*!< SELinux label handle (if any). */

    unsigned fflags;		/*!< File flags. */
    rpmFileAction action;	/*!< File disposition. */
    fileStage goal;		/*!< Package state machine goal. */
    struct stat sb;		/*!< Current file stat(2) info. */
    struct stat osb;		/*!< Original file stat(2) info. */
};


/**
 * Retrieve transaction element file info from file state machine iterator.
 * @param fsm		file state machine
 * @return		transaction element file info
 */
static rpmfi fsmGetFi(const FSM_t fsm)
{
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->fi : NULL);
}

static rpmfs fsmGetFs(const FSM_t fsm)
{
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->fs : NULL);
}

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
 * @param fsm		file state machine data
 * @param isDir		directory or regular path?
 * @param suffix	suffix to use (NULL disables)
 * @retval		path to file (malloced)
 */
static char * fsmFsPath(const FSM_t fsm, int isDir,
			const char * suffix)
{
    return rstrscat(NULL,
		    fsm->dirName, fsm->baseName,
		    (!isDir && suffix) ? suffix : "",
		    NULL);
}

/** \ingroup payload
 * Destroy file info iterator.
 * @param p		file info iterator
 * @retval		NULL always
 */
static FSMI_t mapFreeIterator(FSMI_t iter)
{
    if (iter) {
	iter->fs = NULL; /* rpmfs is not refcounted */
	iter->fi = rpmfiFree(iter->fi);
	free(iter);
    }
    return NULL;
}

/** \ingroup payload
 * Create file info iterator.
 * @param fi		transaction element file info
 * @return		file info iterator
 */
static FSMI_t 
mapInitIterator(rpmfs fs, rpmfi fi, int reverse)
{
    FSMI_t iter = NULL;

    iter = xcalloc(1, sizeof(*iter));
    iter->fs = fs; /* rpmfs is not refcounted */
    iter->fi = rpmfiLink(fi);
    iter->reverse = reverse;
    iter->i = (iter->reverse ? (rpmfiFC(fi) - 1) : 0);
    iter->isave = iter->i;
    return iter;
}

/** \ingroup payload
 * Return next index into file info.
 * @param a		file info iterator
 * @return		next index, -1 on termination
 */
static int mapNextIterator(FSMI_t iter)
{
    int i = -1;

    if (iter) {
	const rpmfi fi = iter->fi;
	if (iter->reverse) {
	    if (iter->i >= 0)	i = iter->i--;
	} else {
    	    if (iter->i < rpmfiFC(fi))	i = iter->i++;
	}
	iter->isave = i;
    }
    return i;
}

/** \ingroup payload
 */
static int cpioStrCmp(const void * a, const void * b)
{
    const char * afn = *(const char **)a;
    const char * bfn = *(const char **)b;

    /* Match rpm-4.0 payloads with ./ prefixes. */
    if (afn[0] == '.' && afn[1] == '/')	afn += 2;
    if (bfn[0] == '.' && bfn[1] == '/')	bfn += 2;

    /* If either path is absolute, make it relative. */
    if (afn[0] == '/')	afn += 1;
    if (bfn[0] == '/')	bfn += 1;

    return strcmp(afn, bfn);
}

/** \ingroup payload
 * Locate archive path in file info.
 * @param iter		file info iterator
 * @param fsmPath	archive path
 * @return		index into file info, -1 if archive path was not found
 */
static int mapFind(FSMI_t iter, const char * fsmPath)
{
    int ix = -1;

    if (iter) {
	const rpmfi fi = iter->fi;
	int fc = rpmfiFC(fi);
	if (fi && fc > 0 && fi->apath && fsmPath && *fsmPath) {
	    char ** p = NULL;

	    if (fi->apath != NULL)
		p = bsearch(&fsmPath, fi->apath, fc, sizeof(fsmPath),
			cpioStrCmp);
	    if (p) {
		iter->i = p - fi->apath;
		ix = mapNextIterator(iter);
	    }
	}
    }
    return ix;
}

/** \ingroup payload
 * Directory name iterator.
 */
typedef struct dnli_s {
    rpmfi fi;
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
static DNLI_t dnlInitIterator(rpmfi fi, rpmfs fs, int reverse)
{
    DNLI_t dnli;
    int i, j;
    int dc;

    if (fi == NULL)
	return NULL;
    dc = rpmfiDC(fi);
    dnli = xcalloc(1, sizeof(*dnli));
    dnli->fi = fi;
    dnli->reverse = reverse;
    dnli->i = (reverse ? dc : 0);

    if (dc) {
	dnli->active = xcalloc(dc, sizeof(*dnli->active));
	int fc = rpmfiFC(fi);

	/* Identify parent directories not skipped. */
	for (i = 0; i < fc; i++)
            if (!XFA_SKIPPING(rpmfsGetAction(fs, i)))
		dnli->active[rpmfiDIIndex(fi, i)] = 1;

	/* Exclude parent directories that are explicitly included. */
	for (i = 0; i < fc; i++) {
	    int dil;
	    size_t dnlen, bnlen;

	    if (!S_ISDIR(rpmfiFModeIndex(fi, i)))
		continue;

	    dil = rpmfiDIIndex(fi, i);
	    dnlen = strlen(rpmfiDNIndex(fi, dil));
	    bnlen = strlen(rpmfiBNIndex(fi, i));

	    for (j = 0; j < dc; j++) {
		const char * dnl;
		size_t jlen;

		if (!dnli->active[j] || j == dil)
		    continue;
		dnl = rpmfiDNIndex(fi, j);
		jlen = strlen(dnl);
		if (jlen != (dnlen+bnlen+1))
		    continue;
		if (!rstreqn(dnl, rpmfiDNIndex(fi, dil), dnlen))
		    continue;
		if (!rstreqn(dnl+dnlen, rpmfiBNIndex(fi, i), bnlen))
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
		rpmlog(RPMLOG_DEBUG, "%10d %s\n", i, rpmfiDNIndex(fi, i));
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
	rpmfi fi = dnli->fi;
	int dc = rpmfiDC(fi);
	int i = -1;

	if (dnli->active)
	do {
	    i = (!dnli->reverse ? dnli->i++ : --dnli->i);
	} while (i >= 0 && i < dc && !dnli->active[i]);

	if (i >= 0 && i < dc)
	    dn = rpmfiDNIndex(fi, i);
	else
	    i = -1;
	dnli->isave = i;
    }
    return dn;
}

/**
 * Map next file path and action.
 * @param fsm		file state machine
 * @param i		file index
 */
static int fsmMapPath(FSM_t fsm, int i)
{
    rpmfi fi = fsmGetFi(fsm);	/* XXX const except for fstates */
    int rc = 0;

    fsm->osuffix = NULL;
    fsm->nsuffix = NULL;
    fsm->action = FA_UNKNOWN;

    if (fi && i >= 0 && i < rpmfiFC(fi)) {
	rpmfs fs = fsmGetFs(fsm);
	/* XXX these should use rpmfiFFlags() etc */
	fsm->action = rpmfsGetAction(fs, i);
	fsm->fflags = rpmfiFFlagsIndex(fi, i);

	/* src rpms have simple base name in payload. */
	fsm->dirName = rpmfiDNIndex(fi, rpmfiDIIndex(fi, i));
	fsm->baseName = rpmfiBNIndex(fi, i);

	/* Never create backup for %ghost files. */
	if (fsm->goal != FSM_PKGBUILD && !(fsm->fflags & RPMFILE_GHOST)) {
	    switch (fsm->action) {
	    case FA_ALTNAME:
		fsm->nsuffix = SUFFIX_RPMNEW;
		break;
	    case FA_SAVE:
		fsm->osuffix = SUFFIX_RPMSAVE;
		break;
	    case FA_BACKUP:
		fsm->osuffix = (fsm->goal == FSM_PKGINSTALL) ?
				SUFFIX_RPMORIG : SUFFIX_RPMSAVE;
		break;
	    default:
		break;
	    }
	}

	if ((fsm->mapFlags & CPIO_MAP_PATH) || fsm->nsuffix) {
	    fsm->path = _free(fsm->path);
	    fsm->path = fsmFsPath(fsm, S_ISDIR(fsm->sb.st_mode),
		(fsm->suffix ? fsm->suffix : fsm->nsuffix));
	}
    }
    return rc;
}

/** \ingroup payload
 * Save hard link in chain.
 * @param fsm		file state machine data
 * @retval linkSet	hard link set when complete
 * @return		Is chain only partially filled?
 */
static int saveHardLink(FSM_t fsm, hardLink_t * linkSet)
{
    struct stat * st = &fsm->sb;
    int rc = 0;
    int ix = -1;
    int j;
    hardLink_t *tailp, li;

    /* Find hard link set. */
    for (tailp = &fsm->links; (li = *tailp) != NULL; tailp = &li->next) {
	if (li->sb.st_ino == st->st_ino && li->sb.st_dev == st->st_dev)
	    break;
    }

    /* New hard link encountered, add new link to set. */
    if (li == NULL) {
	li = xcalloc(1, sizeof(*li));
	li->next = NULL;
	li->sb = *st;	/* structure assignment */
	li->nlink = st->st_nlink;
	li->linkIndex = fsm->ix;
	li->createdPath = -1;

	li->filex = xcalloc(st->st_nlink, sizeof(li->filex[0]));
	memset(li->filex, -1, (st->st_nlink * sizeof(li->filex[0])));
	li->nsuffix = xcalloc(st->st_nlink, sizeof(*li->nsuffix));

	if (fsm->goal == FSM_PKGBUILD)
	    li->linksLeft = st->st_nlink;
	if (fsm->goal == FSM_PKGINSTALL)
	    li->linksLeft = 0;

	*tailp = li;	/* append to tail of linked list */
    }

    if (fsm->goal == FSM_PKGBUILD) --li->linksLeft;
    li->filex[li->linksLeft] = fsm->ix;
    li->nsuffix[li->linksLeft] = fsm->nsuffix;
    if (fsm->goal == FSM_PKGINSTALL) li->linksLeft++;

    if (fsm->goal == FSM_PKGBUILD)
	return (li->linksLeft > 0);

    if (fsm->goal != FSM_PKGINSTALL)
	return 0;

    if (!(st->st_size || li->linksLeft == st->st_nlink))
	return 1;

    /* Here come the bits, time to choose a non-skipped file name. */
    {	rpmfs fs = fsmGetFs(fsm);

	for (j = li->linksLeft - 1; j >= 0; j--) {
	    ix = li->filex[j];
	    if (ix < 0 || XFA_SKIPPING(rpmfsGetAction(fs, ix)))
		continue;
	    break;
	}
    }

    /* Are all links skipped or not encountered yet? */
    if (ix < 0 || j < 0)
	return 1;	/* XXX W2DO? */

    /* Save the non-skipped file name and map index. */
    li->linkIndex = j;
    if (linkSet)
	*linkSet = li;
    fsm->path = _free(fsm->path);
    fsm->ix = ix;
    rc = fsmMapPath(fsm, fsm->ix);
    return rc;
}

/** \ingroup payload
 * Destroy set of hard links.
 * @param li		set of hard links
 * @return		NULL always
 */
static hardLink_t freeHardLink(hardLink_t li)
{
    if (li) {
	li->nsuffix = _free(li->nsuffix);	/* XXX elements are shared */
	li->filex = _free(li->filex);
	_free(li);
    }
    return NULL;
}

/* Check for hard links missing from payload */
static int checkHardLinks(FSM_t fsm)
{
    int rc = 0;
    rpmfs fs = fsmGetFs(fsm);

    for (hardLink_t li = fsm->links; li != NULL; li = li->next) {
	if (li->linksLeft) {
	    for (nlink_t i = 0 ; i < li->linksLeft; i++) {
		int ix = li->filex[i];
		if (ix < 0 || XFA_SKIPPING(rpmfsGetAction(fs, ix)))
		    continue;
		rc = CPIOERR_MISSING_HARDLINK;
		if (fsm->failedFile && *fsm->failedFile == NULL) {
		    if (!fsmMapPath(fsm, ix)) {
			/* Out-of-sync hardlinks handled as sub-state */
			*fsm->failedFile = fsm->path;
			fsm->path = NULL;
		    }
		}
		break;
	    }
	}
    }
    return rc;
}

static FSM_t fsmNew(fileStage goal, rpmfs fs, rpmfi fi, char ** failedFile)
{
    FSM_t fsm = xcalloc(1, sizeof(*fsm));

    fsm->ix = -1;
    fsm->goal = goal;
    fsm->iter = mapInitIterator(fs, fi, (goal == FSM_PKGERASE));

    /* common flags for all modes */
    fsm->mapFlags = CPIO_MAP_PATH | CPIO_MAP_MODE | CPIO_MAP_UID | CPIO_MAP_GID;

    if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
        fsm->bufsize = 8 * BUFSIZ;
        fsm->buf = xmalloc(fsm->bufsize);
    }

    fsm->failedFile = failedFile;
    if (fsm->failedFile)
	*fsm->failedFile = NULL;

    return fsm;
}

static FSM_t fsmFree(FSM_t fsm)
{
    hardLink_t li;
    fsm->buf = _free(fsm->buf);
    fsm->bufsize = 0;

    fsm->iter = mapFreeIterator(fsm->iter);
    fsm->failedFile = NULL;

    fsm->path = _free(fsm->path);
    fsm->suffix = _free(fsm->suffix);

    while ((li = fsm->links) != NULL) {
	fsm->links = li->next;
	li->next = NULL;
	freeHardLink(li);
    }
    free(fsm);
    return NULL;
}

/* Find and set file security context */
static int fsmSetSELabel(struct selabel_handle *sehandle,
			 const char *path, mode_t mode)
{
    int rc = 0;
#if WITH_SELINUX
    if (sehandle) {
	security_context_t scon = NULL;

	if (selabel_lookup_raw(sehandle, &scon, path, mode) == 0) {
	    rc = lsetfilecon(path, scon);

	    if (_fsm_debug) {
		rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n",
			__func__, path, scon,
			(rc < 0 ? strerror(errno) : ""));
	    }

	    if (rc < 0 && errno == EOPNOTSUPP)
		rc = 0;
	}

	freecon(scon);
    }
#endif
    return rc ? CPIOERR_LSETFCON_FAILED : 0;
}

static int fsmSetFCaps(const char *path, const char *captxt)
{
    int rc = 0;
#if WITH_CAP
    if (captxt && *captxt != '\0') {
	cap_t fcaps = cap_from_text(captxt);
	if (fcaps == NULL || cap_set_file(path, fcaps) != 0) {
	    rc = CPIOERR_SETCAP_FAILED;
	}
	cap_free(fcaps);
    } 
#endif
    return rc;
}

/**
 * Map file stat(2) info.
 * @param fsm		file state machine
 */
static int fsmMapAttrs(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    rpmfi fi = fsmGetFi(fsm);
    int i = fsm->ix;

    /* this check is pretty moot,  rpmfi accessors check array bounds etc */
    if (fi && i >= 0 && i < rpmfiFC(fi)) {
	ino_t finalInode = rpmfiFInodeIndex(fi, i);
	mode_t finalMode = rpmfiFModeIndex(fi, i);
	dev_t finalRdev = rpmfiFRdevIndex(fi, i);
	time_t finalMtime = rpmfiFMtimeIndex(fi, i);
	const char *user = rpmfiFUserIndex(fi, i);
	const char *group = rpmfiFGroupIndex(fi, i);
	uid_t uid = 0;
	gid_t gid = 0;

	if (user && rpmugUid(user, &uid)) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rpmlog(RPMLOG_WARNING,
		    _("user %s does not exist - using root\n"), user);
	    finalMode &= ~S_ISUID;      /* turn off suid bit */
	}

	if (group && rpmugGid(group, &gid)) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rpmlog(RPMLOG_WARNING,
		    _("group %s does not exist - using root\n"), group);
	    finalMode &= ~S_ISGID;	/* turn off sgid bit */
	}

	if (fsm->mapFlags & CPIO_MAP_MODE)
	    st->st_mode = (st->st_mode & S_IFMT) | (finalMode & ~S_IFMT);
	if (fsm->mapFlags & CPIO_MAP_TYPE) {
	    st->st_mode = (st->st_mode & ~S_IFMT) | (finalMode & S_IFMT);
	    if ((S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
	    && st->st_nlink == 0)
		st->st_nlink = 1;
	    st->st_ino = finalInode;
	    st->st_rdev = finalRdev;
	    st->st_mtime = finalMtime;
	}
	if (fsm->mapFlags & CPIO_MAP_UID)
	    st->st_uid = uid;
	if (fsm->mapFlags & CPIO_MAP_GID)
	    st->st_gid = gid;
    }
    return 0;
}

/** \ingroup payload
 * Create file from payload stream.
 * @param fsm		file state machine data
 * @param archive	payload archive
 * @return		0 on success
 */
static int expandRegular(FSM_t fsm, rpmpsm psm, rpmcpio_t archive, int nodigest)
{
    FD_t wfd = NULL;
    const struct stat * st = &fsm->sb;
    rpm_loff_t left = st->st_size;
    const unsigned char * fidigest = NULL;
    pgpHashAlgo digestalgo = 0;
    int rc = 0;

    wfd = Fopen(fsm->path, "w.ufdio");
    if (Ferror(wfd)) {
	rc = CPIOERR_OPEN_FAILED;
	goto exit;
    }

    if (!nodigest) {
	rpmfi fi = fsmGetFi(fsm);
	digestalgo = rpmfiDigestAlgo(fi);
	fidigest = rpmfiFDigestIndex(fi, fsm->ix, NULL, NULL);
    }

    if (st->st_size > 0 && fidigest)
	fdInitDigest(wfd, digestalgo, 0);

    while (left) {
        size_t len;
	len = (left > fsm->bufsize ? fsm->bufsize : left);
        if (rpmcpioRead(archive, fsm->buf, len) != len) {
            rc = CPIOERR_READ_FAILED;
	    goto exit;
        }
	if ((Fwrite(fsm->buf, sizeof(*fsm->buf), len, wfd) != len) || Ferror(wfd)) {
	    rc = CPIOERR_WRITE_FAILED;
	    goto exit;
	}

	left -= len;

	/* don't call this with fileSize == fileComplete */
	if (!rc && left)
	    rpmpsmNotify(psm, RPMCALLBACK_INST_PROGRESS, rpmcpioTell(archive));
    }

    if (st->st_size > 0 && fidigest) {
	void * digest = NULL;

	(void) Fflush(wfd);
	fdFiniDigest(wfd, digestalgo, &digest, NULL, 0);

	if (digest != NULL && fidigest != NULL) {
	    size_t diglen = rpmDigestLength(digestalgo);
	    if (memcmp(digest, fidigest, diglen)) {
		rc = CPIOERR_DIGEST_MISMATCH;
            }
	} else {
	    rc = CPIOERR_DIGEST_MISMATCH;
	}
	free(digest);
    }

exit:
    if (wfd) {
	int myerrno = errno;
	Fclose(wfd);
	errno = myerrno;
    }
    return rc;
}

static int fsmReadLink(const char *path,
		       char *buf, size_t bufsize, size_t *linklen)
{
    ssize_t llen = readlink(path, buf, bufsize - 1);
    int rc = CPIOERR_READLINK_FAILED;

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

/** \ingroup payload
 * Write next item to payload stream.
 * @param fsm		file state machine data
 * @param writeData	should data be written?
 * @param archive	payload archive
 * @param ix		file index
 * @return		0 on success
 */
static int writeFile(FSM_t fsm, int writeData, rpmcpio_t archive, int ix)
{
    FD_t rfd = NULL;
    char * path = fsm->path;
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    char * symbuf = NULL;
    rpm_loff_t left;
    int rc = 0;

    st->st_size = (writeData ? ost->st_size : 0);

    if (S_ISDIR(st->st_mode)) {
	st->st_size = 0;
    } else if (S_ISLNK(st->st_mode)) {
	/*
	 * While linux puts the size of a symlink in the st_size field,
	 * I don't think that's a specified standard.
	 */
        size_t linklen;
	rc = fsmReadLink(fsm->path, fsm->buf, fsm->bufsize, &linklen);
	if (rc) goto exit;
	st->st_size = linklen;
	rstrcat(&symbuf, fsm->buf);	/* XXX save readlink return. */
    }

    if (fsm->mapFlags & CPIO_MAP_ABSOLUTE) {
	fsm->path = rstrscat(NULL, (fsm->mapFlags & CPIO_MAP_ADDDOT) ? "." : "",
				   fsm->dirName, fsm->baseName, NULL);
    } else if (fsm->mapFlags & CPIO_MAP_PATH) {
	rpmfi fi = fsmGetFi(fsm);
	fsm->path = xstrdup((fi->apath ? fi->apath[ix] : 
					 rpmfiBNIndex(fi, ix)));
    }

    rc = rpmcpioHeaderWrite(archive, fsm->path, st);
    _free(fsm->path);
    fsm->path = path;

    if (rc) goto exit;


    if (writeData && S_ISREG(st->st_mode)) {
	size_t len;

	rfd = Fopen(fsm->path, "r.ufdio");
	if (Ferror(rfd)) {
	    rc = CPIOERR_OPEN_FAILED;
	    goto exit;
	}
	
	left = st->st_size;

	while (left) {
	    len = (left > fsm->bufsize ? fsm->bufsize : left);
	    if (Fread(fsm->buf, sizeof(*fsm->buf), len, rfd) != len || Ferror(rfd)) {
		rc = CPIOERR_READ_FAILED;
		goto exit;
	    }

	    if (rpmcpioWrite(archive, fsm->buf, len) != len) {
		rc = CPIOERR_WRITE_FAILED;
		goto exit;
	    }
	    left -= len;
	}
    } else if (writeData && S_ISLNK(st->st_mode)) {
        size_t len = strlen(symbuf);
        if (rpmcpioWrite(archive, symbuf, len) != len) {
            rc = CPIOERR_WRITE_FAILED;
            goto exit;
        }
    }

exit:
    if (rfd) {
	/* preserve any prior errno across close */
	int myerrno = errno;
	Fclose(rfd);
	errno = myerrno;
    }
    fsm->path = path;
    free(symbuf);
    return rc;
}

/** \ingroup payload
 * Write set of linked files to payload stream.
 * @param fsm		file state machine data
 * @param archive	payload archive
 * @param li		link to write
 * @return		0 on success
 */
static int writeLinkedFile(FSM_t fsm, rpmcpio_t archive, hardLink_t li)
{
    char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    int ec = 0;
    int rc;
    int i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;

    for (i = li->nlink - 1; i >= 0; i--) {

	if (li->filex[i] < 0) continue;

	rc = fsmMapPath(fsm, li->filex[i]);

	/* Write data after last link. */
	rc = writeFile(fsm, (i == 0), archive, li->filex[i]);
	if (fsm->failedFile && rc != 0 && *fsm->failedFile == NULL) {
	    ec = rc;
	    *fsm->failedFile = xstrdup(fsm->path);
	}

	fsm->path = _free(fsm->path);
	li->filex[i] = -1;
    }

    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return ec;
}

static int writeLinks(FSM_t fsm, rpmcpio_t archive)
{
    int j, rc = 0;
    nlink_t i, nlink;

    for (hardLink_t li = fsm->links; li; li = li->next) {
	/* Re-calculate link count for archive header. */
	for (j = -1, nlink = 0, i = 0; i < li->nlink; i++) {
	    if (li->filex[i] < 0)
		continue;
	    nlink++;
	    if (j == -1) j = i;
	}
	/* XXX force the contents out as well. */
	if (j != 0) {
	    li->filex[0] = li->filex[j];
	    li->filex[j] = -1;
	}
	li->sb.st_nlink = nlink;

	fsm->sb = li->sb;	/* structure assignment */
	fsm->osb = fsm->sb;	/* structure assignment */

	if (!rc) rc = writeLinkedFile(fsm, archive, li);
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
        rc = (errno == ENOENT ? CPIOERR_ENOENT : CPIOERR_LSTAT_FAILED);
	/* WTH is this, and is it really needed, still? */
        memset(sb, 0, sizeof(*sb));	/* XXX s390x hackery */
    }
    return rc;
}

static int fsmVerify(FSM_t fsm);

/** \ingroup payload
 * Create pending hard links to existing file.
 * @param fsm		file state machine data
 * @param li		hard link
 * @return		0 on success
 */
static int fsmMakeLinks(FSM_t fsm, hardLink_t li)
{
    char * path = fsm->path;
    char * opath = NULL;
    const char * nsuffix = fsm->nsuffix;
    int ec = 0;
    int rc;
    int i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;

    rc = fsmMapPath(fsm, li->filex[li->createdPath]);
    opath = fsm->path;
    fsm->path = NULL;
    for (i = 0; i < li->nlink; i++) {
	if (li->filex[i] < 0) continue;
	if (li->createdPath == i) continue;

	fsm->path = _free(fsm->path);
	rc = fsmMapPath(fsm, li->filex[i]);
	if (XFA_SKIPPING(fsm->action)) continue;

	rc = fsmVerify(fsm);
	if (!rc) continue;
	if (!(rc == CPIOERR_ENOENT)) break;

	/* XXX link(opath, fsm->path) */
	rc = link(opath, fsm->path);
	if (_fsm_debug)
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", __func__,
		opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_LINK_FAILED;

	if (fsm->failedFile && rc != 0 && *fsm->failedFile == NULL) {
	    ec = rc;
	    *fsm->failedFile = xstrdup(fsm->path);
	}

	li->linksLeft--;
    }
    fsm->path = _free(fsm->path);
    free(opath);

    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return ec;
}

static int fsmCommit(FSM_t fsm, int ix);

/** \ingroup payload
 * Commit hard linked file set atomically.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int fsmCommitLinks(FSM_t fsm)
{
    char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    struct stat * st = &fsm->sb;
    int rc = 0;
    nlink_t i;
    hardLink_t li;

    fsm->path = NULL;
    fsm->nsuffix = NULL;

    for (li = fsm->links; li != NULL; li = li->next) {
	if (li->sb.st_ino == st->st_ino && li->sb.st_dev == st->st_dev)
	    break;
    }

    for (i = 0; i < li->nlink; i++) {
	if (li->filex[i] < 0) continue;
	rc = fsmMapPath(fsm, li->filex[i]);
	if (!XFA_SKIPPING(fsm->action))
	    rc = fsmCommit(fsm, li->filex[i]);
	fsm->path = _free(fsm->path);
	li->filex[i] = -1;
    }

    fsm->nsuffix = nsuffix;
    fsm->path = path;
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
	case ENOENT:        rc = CPIOERR_ENOENT;    break;
	case ENOTEMPTY:     rc = CPIOERR_ENOTEMPTY; break;
	default:            rc = CPIOERR_RMDIR_FAILED; break;
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
    if (rc < 0)	rc = CPIOERR_MKDIR_FAILED;
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
	rc = CPIOERR_MKFIFO_FAILED;

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
	rc = CPIOERR_MKNOD_FAILED;

    return rc;
}

/**
 * Create (if necessary) directories not explicitly included in package.
 * @param dnli		file state machine data
 * @param sehandle	selinux label handle (bah)
 * @return		0 on success
 */
static int fsmMkdirs(rpmfi fi, rpmfs fs, struct selabel_handle *sehandle)
{
    DNLI_t dnli = dnlInitIterator(fi, fs, 0);
    struct stat sb;
    const char *dpath;
    int dc = rpmfiDC(fi);
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
	    } else if (rc == CPIOERR_ENOENT) {
		*te = '\0';
		mode_t mode = S_IFDIR | (_dirPerms & 07777);
		rc = fsmMkdir(dn, mode);
		if (!rc) {
		    rc = fsmSetSELabel(sehandle, dn, mode);

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

/********************************************************************/

static void fsmReset(FSM_t fsm)
{
    fsm->path = _free(fsm->path);
    fsm->postpone = 0;
    fsm->diskchecked = fsm->exists = 0;
    fsm->action = FA_UNKNOWN;
    fsm->osuffix = NULL;
    fsm->nsuffix = NULL;
    memset(&(fsm->sb), 0, sizeof(fsm->sb));
    memset(&(fsm->osb), 0, sizeof(fsm->sb));
}

static int fsmInit(FSM_t fsm)
{
    int rc = 0;

    /* On non-install, mode must be known so that dirs don't get suffix. */
    if (fsm->goal != FSM_PKGINSTALL) {
	rpmfi fi = fsmGetFi(fsm);
	fsm->sb.st_mode = rpmfiFModeIndex(fi, fsm->ix);
    }

    /* Generate file path. */
    rc = fsmMapPath(fsm, fsm->ix);
    if (rc) return rc;

    /* Perform lstat/stat for disk file. */
    if (fsm->path != NULL &&
	!(fsm->goal == FSM_PKGINSTALL && S_ISREG(fsm->sb.st_mode)))
    {
	int dolstat = !(fsm->mapFlags & CPIO_FOLLOW_SYMLINKS);
	rc = fsmStat(fsm->path, dolstat, &fsm->osb);
	if (rc == CPIOERR_ENOENT) {
	    // errno = saveerrno; XXX temporary commented out
	    rc = 0;
	    fsm->exists = 0;
	} else if (rc == 0) {
	    fsm->exists = 1;
	}
    } else {
	/* Skip %ghost files on build. */
	fsm->exists = 0;
    }
    fsm->diskchecked = 1;
    if (rc) return rc;

    /* On non-install, the disk file stat is what's remapped. */
    if (fsm->goal != FSM_PKGINSTALL)
	fsm->sb = fsm->osb;			/* structure assignment */

    /* Remap file perms, owner, and group. */
    rc = fsmMapAttrs(fsm);
    if (rc) return rc;

    fsm->postpone = XFA_SKIPPING(fsm->action);

    rpmlog(RPMLOG_DEBUG, "%-10s %06o%3d (%4d,%4d)%6d %s\n",
	   fileActionString(fsm->action), (int)fsm->sb.st_mode,
	   (int)fsm->sb.st_nlink, (int)fsm->sb.st_uid,
	   (int)fsm->sb.st_gid, (int)fsm->sb.st_size,
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
	rc = CPIOERR_SYMLINK_FAILED;
    return rc;
}

static int fsmUnlink(const char *path, cpioMapFlags mapFlags)
{
    int rc = 0;
    if (mapFlags & CPIO_SBIT_CHECK)
        removeSBITS(path);
    rc = unlink(path);
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s) %s\n", __func__,
	       path, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)
	rc = (errno == ENOENT ? CPIOERR_ENOENT : CPIOERR_UNLINK_FAILED);
    return rc;
}

static int fsmRename(const char *opath, const char *path,
		     cpioMapFlags mapFlags)
{
    if (mapFlags & CPIO_SBIT_CHECK)
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
    if (rc < 0)	rc = CPIOERR_RENAME_FAILED;
    return rc;
}


static int fsmChown(const char *path, uid_t uid, gid_t gid)
{
    int rc = chown(path, uid, gid);
    if (rc < 0) {
	struct stat st;
	if (lstat(path, &st) == 0 && st.st_uid == uid && st.st_gid == gid)
	    rc = 0;
    }
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, %d) %s\n", __func__,
	       path, (int)uid, (int)gid,
	       (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
    return rc;
}

static int fsmLChown(const char *path, uid_t uid, gid_t gid)
{
    int rc = lchown(path, uid, gid);
    if (rc < 0) {
	struct stat st;
	if (lstat(path, &st) == 0 && st.st_uid == uid && st.st_gid == gid)
	    rc = 0;
    }
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, %d) %s\n", __func__,
	       path, (int)uid, (int)gid,
	       (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
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
    if (rc < 0)	rc = CPIOERR_CHMOD_FAILED;
    return rc;
}

static int fsmUtime(const char *path, time_t mtime)
{
    int rc = 0;
    struct utimbuf stamp;
    stamp.actime = mtime;
    stamp.modtime = mtime;
    rc = utime(path, &stamp);
    if (_fsm_debug)
	rpmlog(RPMLOG_DEBUG, " %8s (%s, 0x%x) %s\n", __func__,
	       path, (unsigned)mtime, (rc < 0 ? strerror(errno) : ""));
    if (rc < 0)	rc = CPIOERR_UTIME_FAILED;
    return rc;
}

static int fsmVerify(FSM_t fsm)
{
    int rc;
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    int saveerrno = errno;

    if (fsm->diskchecked && !fsm->exists) {
        return CPIOERR_ENOENT;
    }
    if (S_ISREG(st->st_mode)) {
	/* HP-UX (and other os'es) don't permit unlink on busy files. */
	char *rmpath = rstrscat(NULL, fsm->path, "-RPMDELETE", NULL);
	rc = fsmRename(fsm->path, rmpath, fsm->mapFlags);
	/* XXX shouldn't we take unlink return code here? */
	if (!rc)
	    (void) fsmUnlink(rmpath, fsm->mapFlags);
	else
	    rc = CPIOERR_UNLINK_FAILED;
	free(rmpath);
        return (rc ? rc : CPIOERR_ENOENT);	/* XXX HACK */
    } else if (S_ISDIR(st->st_mode)) {
        if (S_ISDIR(ost->st_mode)) return 0;
        if (S_ISLNK(ost->st_mode)) {
            rc = fsmStat(fsm->path, 0, &fsm->osb);
            if (rc == CPIOERR_ENOENT) rc = 0;
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
	    /* FSM_PROCESS puts link target to fsm->buf. */
            if (rstreq(fsm->buf, buf)) return 0;
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
    rc = fsmUnlink(fsm->path, fsm->mapFlags);
    if (rc == 0)	rc = CPIOERR_ENOENT;
    return (rc ? rc : CPIOERR_ENOENT);	/* XXX HACK */
}

#define	IS_DEV_LOG(_x)	\
	((_x) != NULL && strlen(_x) >= (sizeof("/dev/log")-1) && \
	rstreqn((_x), "/dev/log", sizeof("/dev/log")-1) && \
	((_x)[sizeof("/dev/log")-1] == '\0' || \
	 (_x)[sizeof("/dev/log")-1] == ';'))



/* Rename pre-existing modified or unmanaged file. */
static int fsmBackup(FSM_t fsm)
{
    int rc = 0;

    /* FIXME: %ghost can have backup action but no suffix */
    if ((fsm->action == FA_SAVE || fsm->action == FA_BACKUP) && fsm->osuffix) {
        char * opath = fsmFsPath(fsm, S_ISDIR(fsm->sb.st_mode), NULL);
        char * path = fsmFsPath(fsm, 0, fsm->osuffix);
        rc = fsmRename(opath, path, fsm->mapFlags);
        if (!rc) {
            rpmlog(RPMLOG_WARNING, _("%s saved as %s\n"), opath, path);
            fsm->exists = 0; /* it doesn't exist anymore... */
        }
        free(path);
        free(opath);
    }
    return rc;
}

static int fsmCommit(FSM_t fsm, int ix)
{
    int rc = 0;
    struct stat * st = &fsm->sb;

    /* XXX Special case /dev/log, which shouldn't be packaged anyways */
    if (!S_ISSOCK(st->st_mode) && !IS_DEV_LOG(fsm->path)) {
	/* Backup on-disk file if needed. Directories are handled earlier */
	if (!S_ISDIR(st->st_mode))
	    rc = fsmBackup(fsm);
        /* Rename temporary to final file name. */
        if (!S_ISDIR(st->st_mode) && (fsm->suffix || fsm->nsuffix)) {
            char *npath = fsmFsPath(fsm, 0, fsm->nsuffix);
            rc = fsmRename(fsm->path, npath, fsm->mapFlags);
            if (!rc && fsm->nsuffix) {
                char * opath = fsmFsPath(fsm, 0, NULL);
                rpmlog(RPMLOG_WARNING, _("%s created as %s\n"),
                       opath, npath);
                free(opath);
            }
            free(fsm->path);
            fsm->path = npath;
        }
        /* Set file security context (if enabled) */
        if (!rc && !getuid()) {
            rc = fsmSetSELabel(fsm->sehandle, fsm->path, fsm->sb.st_mode);
        }
        if (S_ISLNK(st->st_mode)) {
            if (!rc && !getuid())
                rc = fsmLChown(fsm->path, fsm->sb.st_uid, fsm->sb.st_gid);
        } else {
            rpmfi fi = fsmGetFi(fsm);
            if (!rc && !getuid())
                rc = fsmChown(fsm->path, fsm->sb.st_uid, fsm->sb.st_gid);
            if (!rc)
                rc = fsmChmod(fsm->path, fsm->sb.st_mode);
            if (!rc) {
                rc = fsmUtime(fsm->path, rpmfiFMtimeIndex(fi, ix));
                /* utime error is not critical for directories */
                if (rc && S_ISDIR(st->st_mode))
                    rc = 0;
            }
            /* Set file capabilities (if enabled) */
            if (!rc && !S_ISDIR(st->st_mode) && !getuid()) {
                rc = fsmSetFCaps(fsm->path, rpmfiFCapsIndex(fi, ix));
            }
        }
    }

    if (rc && fsm->failedFile && *fsm->failedFile == NULL) {
        *fsm->failedFile = fsm->path;
        fsm->path = NULL;
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
    case FA_COPYOUT:	return "copyout";
    case FA_COPYIN:	return "copyin";
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
static void setFileState(rpmfs fs, int i, rpmFileAction action)
{
    switch (action) {
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

int rpmPackageFilesInstall(rpmts ts, rpmte te, rpmfi fi, FD_t cfd,
              rpmpsm psm, char ** failedFile)
{
    rpmfs fs = rpmteGetFileStates(te);
    FSM_t fsm = fsmNew(FSM_PKGINSTALL, fs, fi, failedFile);
    rpmcpio_t archive = rpmcpioOpen(cfd, O_RDONLY);
    struct stat * st = &fsm->sb;
    int saveerrno = errno;
    int rc = 0;
    int nodigest = (rpmtsFlags(ts) & RPMTRANS_FLAG_NOFILEDIGEST);

    if (!rpmteIsSource(te))
	fsm->mapFlags |= CPIO_SBIT_CHECK;

    if (archive == NULL)
	rc = CPIOERR_INTERNAL;

    fsm->sehandle = rpmtsSELabelHandle(ts);
    /* transaction id used for temporary path suffix while installing */
    rasprintf(&fsm->suffix, ";%08x", (unsigned)rpmtsGetTid(ts));

    /* Detect and create directories not explicitly in package. */
    if (!rc) {
	rc = fsmMkdirs(fi, rpmteGetFileStates(te), fsm->sehandle);
    }

    while (!rc) {
	hardLink_t li = NULL;

        /* Clean fsm, free'ing memory. */
	fsmReset(fsm);

	/* Read next payload header. */
        rc = rpmcpioHeaderRead(archive, &(fsm->path), &(fsm->sb));

	/* Detect and exit on end-of-payload. */
	if (rc == CPIOERR_HDR_TRAILER) {
	    rc = 0;
	    break;
	}

	if (rc) break;

	/* Identify mapping index. */
	fsm->ix = mapFind(fsm->iter, fsm->path);

	/* Mapping error */
	if (fsm->ix < 0) {
	    if (fsm->failedFile && *fsm->failedFile == NULL)
		*fsm->failedFile = xstrdup(fsm->path);
	    rc = CPIOERR_UNMAPPED_FILE;
	    break;
	}

        rc = fsmInit(fsm);

        /* Exit on error. */
        if (rc) {
            fsm->postpone = 1;
            break;
        }

	if (S_ISREG(fsm->sb.st_mode) && fsm->sb.st_nlink > 1)
	    fsm->postpone = saveHardLink(fsm, &li);

	setFileState(rpmteGetFileStates(te), fsm->ix, fsm->action);

        if (!fsm->postpone) {
            if (S_ISREG(st->st_mode)) {
                rc = fsmVerify(fsm);
                if (!(rc == CPIOERR_ENOENT)) return rc;
                rc = expandRegular(fsm, psm, archive, nodigest);
            } else if (S_ISDIR(st->st_mode)) {
		/* Directories replacing something need early backup */
                rc = fsmBackup(fsm);
                rc = fsmVerify(fsm);
                if (rc == CPIOERR_ENOENT) {
                    mode_t mode = st->st_mode;
                    mode &= ~07777;
                    mode |=  00700;
                    rc = fsmMkdir(fsm->path, mode);
                }
            } else if (S_ISLNK(st->st_mode)) {
                if ((st->st_size + 1) > fsm->bufsize) {
                    rc = CPIOERR_HDR_SIZE;
                } else if (rpmcpioRead(archive, fsm->buf, st->st_size) != st->st_size) {
                    rc = CPIOERR_READ_FAILED;
                } else {

                    fsm->buf[st->st_size] = '\0';
                    /* fsmVerify() assumes link target in fsm->buf */
                    rc = fsmVerify(fsm);
                    if (rc == CPIOERR_ENOENT) {
                        rc = fsmSymlink(fsm->buf, fsm->path);
                    }
                }
            } else if (S_ISFIFO(st->st_mode)) {
                /* This mimics cpio S_ISSOCK() behavior but probably isn't right */
                rc = fsmVerify(fsm);
                if (rc == CPIOERR_ENOENT) {
                    rc = fsmMkfifo(fsm->path, 0000);
                }
            } else if (S_ISCHR(st->st_mode) ||
                       S_ISBLK(st->st_mode) ||
                       S_ISSOCK(st->st_mode))
            {
                rc = fsmVerify(fsm);
                if (rc == CPIOERR_ENOENT) {
                    rc = fsmMknod(fsm->path, fsm->sb.st_mode, fsm->sb.st_rdev);
                }
            } else {
                /* XXX Special case /dev/log, which shouldn't be packaged anyways */
                if (!IS_DEV_LOG(fsm->path))
                    rc = CPIOERR_UNKNOWN_FILETYPE;
            }
            if (li != NULL) {
                li->createdPath = li->linkIndex;
                rc = fsmMakeLinks(fsm, li);
            }
        }

        if (rc) {
            if (!fsm->postpone) {
                /* XXX only erase if temp fn w suffix is in use */
                if (fsm->suffix) {
                    if (S_ISDIR(st->st_mode)) {
                        (void) fsmRmdir(fsm->path);
                    } else {
                        (void) fsmUnlink(fsm->path, fsm->mapFlags);
                    }
                }
                errno = saveerrno;
                if (fsm->failedFile && *fsm->failedFile == NULL)
                    *fsm->failedFile = xstrdup(fsm->path);
            }

            break;
        }

        /* Notify on success. */
        rpmpsmNotify(psm, RPMCALLBACK_INST_PROGRESS, rpmcpioTell(archive));

        if (!fsm->postpone) {
            rc = ((S_ISREG(st->st_mode) && st->st_nlink > 1)
                  ? fsmCommitLinks(fsm) : fsmCommit(fsm, fsm->ix));
        }
        if (rc) {
            break;
        }
    }

    if (!rc)
	rc = checkHardLinks(fsm);

    /* No need to bother with close errors on read */
    rpmcpioFree(archive);
    fsmFree(fsm);

    return rc;
}


int rpmPackageFilesRemove(rpmts ts, rpmte te, rpmfi fi,
              rpmpsm psm, char ** failedFile)
{
    rpmfs fs = rpmteGetFileStates(te);
    FSM_t fsm = fsmNew(FSM_PKGERASE, fs, fi, failedFile);
    int rc = 0;

    if (!rpmteIsSource(te))
	fsm->mapFlags |= CPIO_SBIT_CHECK;

    while (!rc) {
        /* Clean fsm, free'ing memory. */
	fsmReset(fsm);

	/* Identify mapping index. */
	fsm->ix = mapNextIterator(fsm->iter);

        /* Exit on end-of-payload. */
        if (fsm->ix < 0)
            break;

        rc = fsmInit(fsm);

	if (!fsm->postpone)
	    rc = fsmBackup(fsm);

        /* Remove erased files. */
        if (!fsm->postpone && fsm->action == FA_ERASE) {
	    int missingok = (fsm->fflags & (RPMFILE_MISSINGOK | RPMFILE_GHOST));

            if (S_ISDIR(fsm->sb.st_mode)) {
                rc = fsmRmdir(fsm->path);
            } else {
                rc = fsmUnlink(fsm->path, fsm->mapFlags);
	    }

	    /*
	     * Missing %ghost or %missingok entries are not errors.
	     * XXX: Are non-existent files ever an actual error here? Afterall
	     * that's exactly what we're trying to accomplish here,
	     * and complaining about job already done seems like kinderkarten
	     * level "But it was MY turn!" whining...
	     */
	    if (rc == CPIOERR_ENOENT && missingok) {
		rc = 0;
	    }

	    /*
	     * Dont whine on non-empty directories for now. We might be able
	     * to track at least some of the expected failures though,
	     * such as when we knowingly left config file backups etc behind.
	     */
	    if (rc == CPIOERR_ENOTEMPTY) {
		rc = 0;
	    }

	    if (rc) {
		int lvl = strict_erasures ? RPMLOG_ERR : RPMLOG_WARNING;
		rpmlog(lvl, _("%s %s: remove failed: %s\n"),
			S_ISDIR(fsm->sb.st_mode) ? _("directory") : _("file"),
			fsm->path, strerror(errno));
            }
        }
        /* XXX Failure to remove is not (yet) cause for failure. */
        if (!strict_erasures) rc = 0;

        if (rc) break;

        /* Notify on success. */
        /* On erase we're iterating backwards, fixup for progress */
        rpm_loff_t amount = (fsm->ix >= 0) ?
            rpmfiFC(fsmGetFi(fsm)) - fsm->ix : 0;
        rpmpsmNotify(psm, RPMCALLBACK_UNINST_PROGRESS, amount);
    }

    fsmFree(fsm);

    return rc;
}


int rpmPackageFilesArchive(rpmfi fi, int isSrc, FD_t cfd,
              rpm_loff_t * archiveSize, char ** failedFile)
{
    rpmfs fs = rpmfsNew(rpmfiFC(fi), 0);;
    FSM_t fsm = fsmNew(FSM_PKGBUILD, fs, fi, failedFile);;
    rpmcpio_t archive = rpmcpioOpen(cfd, O_WRONLY);
    int rc = 0;

    fsm->mapFlags |= CPIO_MAP_TYPE;
    if (isSrc) 
	fsm->mapFlags |= CPIO_FOLLOW_SYMLINKS;

    if (archive == NULL) {
	rc = CPIOERR_INTERNAL;
    } else {
	int ghost, i, fc = rpmfiFC(fi);

	/* XXX Is this actually still needed? */
	for (i = 0; i < fc; i++) {
	    ghost = (rpmfiFFlagsIndex(fi, i) & RPMFILE_GHOST);
	    rpmfsSetAction(fs, i, ghost ? FA_SKIP : FA_COPYOUT);
	}
    }
	    
    while (!rc) {
	fsmReset(fsm);

	/* Identify mapping index. */
	fsm->ix = mapNextIterator(fsm->iter);

        /* Exit on end-of-payload. */
        if (fsm->ix < 0)
            break;

        rc = fsmInit(fsm);

        /* Exit on error. */
        if (rc) {
            fsm->postpone = 1;
            break;
        }

	if (fsm->fflags & RPMFILE_GHOST) /* XXX Don't if %ghost file. */
	    continue;

	if (S_ISREG(fsm->sb.st_mode) && fsm->sb.st_nlink > 1)
	    fsm->postpone = saveHardLink(fsm, NULL);

        if (fsm->postpone)
            continue;
        /* Hardlinks are handled later */
        if (!(S_ISREG(fsm->sb.st_mode) && fsm->sb.st_nlink > 1)) {
            /* Copy file into archive. */
            rc = writeFile(fsm, 1, archive, fsm->ix);
        }

        if (rc) {
            if (!fsm->postpone) {
                if (fsm->failedFile && *fsm->failedFile == NULL)
                    *fsm->failedFile = xstrdup(fsm->path);
            }

            break;
        }
    }

    /* Flush partial sets of hard linked files. */
    if (!rc)
        rc = writeLinks(fsm, archive);

    /* Finish the payload stream */
    if (!rc)
	rc = rpmcpioClose(archive);

    if (archiveSize)
	*archiveSize = (rc == 0) ? rpmcpioTell(archive) : 0;

    rpmcpioFree(archive);
    rpmfsFree(fs);
    fsmFree(fsm);

    return rc;
}
