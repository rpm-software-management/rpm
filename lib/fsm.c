/** \ingroup payload
 * \file lib/fsm.c
 * File state machine to handle a payload from a package.
 */

#include "system.h"

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
#include "lib/misc.h"		/* XXX unameToUid() and gnameToGid() */

#include "debug.h"

#define	_FSM_DEBUG	0
int _fsm_debug = _FSM_DEBUG;

int _fsm_threads = 0;

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
    rpmts ts;			/*!< transaction set. */
    rpmte te;			/*!< transaction element. */
    rpmfi fi;			/*!< transaction element file info. */
    int reverse;		/*!< reversed traversal? */
    int isave;			/*!< last returned iterator index. */
    int i;			/*!< iterator index. */
};

/**
 * Retrieve transaction set from file state machine iterator.
 * @param fsm		file state machine
 * @return		transaction set
 */
static rpmts fsmGetTs(const FSM_t fsm) {
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->ts : NULL);
}

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

static rpmte fsmGetTe(const FSM_t fsm)
{
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->te : NULL);
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
static const char * fileStageString(fileStage a);
static const char * fileActionString(rpmFileAction a);
static int fsmStage(FSM_t fsm, fileStage stage);

/** \ingroup payload
 * Build path to file from file info, ornamented with subdir and suffix.
 * @param fsm		file state machine data
 * @param st		file stat info
 * @param subdir	subdir to use (NULL disables)
 * @param suffix	suffix to use (NULL disables)
 * @retval		path to file (malloced)
 */
static char * fsmFsPath(const FSM_t fsm,
		const struct stat * st,
		const char * subdir,
		const char * suffix)
{
    char * s = NULL;

    if (fsm) {
	size_t nb;
	char * t;
	nb = strlen(fsm->dirName) +
	    (st && !S_ISDIR(st->st_mode) ? (subdir ? strlen(subdir) : 0) : 0) +
	    (st && !S_ISDIR(st->st_mode) ? (suffix ? strlen(suffix) : 0) : 0) +
	    strlen(fsm->baseName) + 1;
	s = t = xmalloc(nb);
	t = stpcpy(t, fsm->dirName);
	if (st && !S_ISDIR(st->st_mode))
	    if (subdir) t = stpcpy(t, subdir);
	t = stpcpy(t, fsm->baseName);
	if (st && !S_ISDIR(st->st_mode))
	    if (suffix) t = stpcpy(t, suffix);
    }
    return s;
}

/** \ingroup payload
 * Destroy file info iterator.
 * @param p		file info iterator
 * @retval		NULL always
 */
static void * mapFreeIterator(void * p)
{
    FSMI_t iter = p;
    if (iter) {
/* XXX rpmswExit() */
	iter->ts = rpmtsFree(iter->ts);
	iter->te = NULL; /* XXX rpmte is not refcounted yet */
	iter->fi = rpmfiUnlink(iter->fi, RPMDBG_M("mapFreeIterator"));
    }
    return _free(p);
}

/** \ingroup payload
 * Create file info iterator.
 * @param ts		transaction set
 * @param fi		transaction element file info
 * @return		file info iterator
 */
static void *
mapInitIterator(rpmts ts, rpmte te, rpmfi fi)
{
    FSMI_t iter = NULL;

    iter = xcalloc(1, sizeof(*iter));
    iter->ts = rpmtsLink(ts, RPMDBG_M("mapIterator"));
    iter->te = te; /* XXX rpmte is not refcounted yet */
    iter->fi = rpmfiLink(fi, RPMDBG_M("mapIterator"));
    iter->reverse = (rpmteType(te) == TR_REMOVED);
    iter->i = (iter->reverse ? (rpmfiFC(fi) - 1) : 0);
    iter->isave = iter->i;
    return iter;
}

/** \ingroup payload
 * Return next index into file info.
 * @param a		file info iterator
 * @return		next index, -1 on termination
 */
static int mapNextIterator(void * a)
{
    FSMI_t iter = a;
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

    /* XXX Some 4+ year old rpm packages have basename only in payloads. */
#ifdef	VERY_OLD_BUGGY_RPM_PACKAGES
    if (strchr(afn, '/') == NULL)
	bfn = strrchr(bfn, '/') + 1;
#endif

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
static void * dnlFreeIterator(void * a)
{
    if (a) {
	DNLI_t dnli = (void *)a;
	if (dnli->active) free(dnli->active);
    }
    return _free(a);
}

/** \ingroup payload
 */
static inline int dnlCount(const DNLI_t dnli)
{
    return (dnli ? rpmfiDC(dnli->fi) : 0);
}

/** \ingroup payload
 */
static inline int dnlIndex(const DNLI_t dnli)
{
    return (dnli ? dnli->isave : -1);
}

/** \ingroup payload
 * Create directory name iterator.
 * @param fsm		file state machine data
 * @param reverse	traverse directory names in reverse order?
 * @return		directory name iterator
 */
static
void * dnlInitIterator(const FSM_t fsm,
		int reverse)
	
{
    rpmfi fi = fsmGetFi(fsm);
    rpmfs fs = rpmteGetFileStates(fsmGetTe(fsm));
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

static void * fsmThread(void * arg)
{
    FSM_t fsm = arg;
    return ((void *) ((long) fsmStage(fsm, fsm->nstage)));
}

int fsmNext(FSM_t fsm, fileStage nstage)
{
    fsm->nstage = nstage;
    if (_fsm_threads)
	return rpmsqJoin( rpmsqThread(fsmThread, fsm) );
    return fsmStage(fsm, fsm->nstage);
}

/** \ingroup payload
 * Save hard link in chain.
 * @param fsm		file state machine data
 * @return		Is chain only partially filled?
 */
static int saveHardLink(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    int rc = 0;
    int ix = -1;
    int j;

    /* Find hard link set. */
    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->sb.st_ino == st->st_ino && fsm->li->sb.st_dev == st->st_dev)
	    break;
    }

    /* New hard link encountered, add new link to set. */
    if (fsm->li == NULL) {
	fsm->li = xcalloc(1, sizeof(*fsm->li));
	fsm->li->next = NULL;
	fsm->li->sb = *st;	/* structure assignment */
	fsm->li->nlink = st->st_nlink;
	fsm->li->linkIndex = fsm->ix;
	fsm->li->createdPath = -1;

	fsm->li->filex = xcalloc(st->st_nlink, sizeof(fsm->li->filex[0]));
	memset(fsm->li->filex, -1, (st->st_nlink * sizeof(fsm->li->filex[0])));
	fsm->li->nsuffix = xcalloc(st->st_nlink, sizeof(*fsm->li->nsuffix));

	if (fsm->goal == FSM_PKGBUILD)
	    fsm->li->linksLeft = st->st_nlink;
	if (fsm->goal == FSM_PKGINSTALL)
	    fsm->li->linksLeft = 0;

	fsm->li->next = fsm->links;
	fsm->links = fsm->li;
    }

    if (fsm->goal == FSM_PKGBUILD) --fsm->li->linksLeft;
    fsm->li->filex[fsm->li->linksLeft] = fsm->ix;
    fsm->li->nsuffix[fsm->li->linksLeft] = fsm->nsuffix;
    if (fsm->goal == FSM_PKGINSTALL) fsm->li->linksLeft++;

    if (fsm->goal == FSM_PKGBUILD)
	return (fsm->li->linksLeft > 0);

    if (fsm->goal != FSM_PKGINSTALL)
	return 0;

    if (!(st->st_size || fsm->li->linksLeft == st->st_nlink))
	return 1;

    /* Here come the bits, time to choose a non-skipped file name. */
    {	rpmfs fs = rpmteGetFileStates(fsmGetTe(fsm));

	for (j = fsm->li->linksLeft - 1; j >= 0; j--) {
	    ix = fsm->li->filex[j];
	    if (ix < 0 || XFA_SKIPPING(rpmfsGetAction(fs, ix)))
		continue;
	    break;
	}
    }

    /* Are all links skipped or not encountered yet? */
    if (ix < 0 || j < 0)
	return 1;	/* XXX W2DO? */

    /* Save the non-skipped file name and map index. */
    fsm->li->linkIndex = j;
    fsm->path = _constfree(fsm->path);
    fsm->ix = ix;
    rc = fsmNext(fsm, FSM_MAP);
    return rc;
}

/** \ingroup payload
 * Destroy set of hard links.
 * @param li		set of hard links
 * @return		NULL always
 */
static void * freeHardLink(hardLink_t li)
{
    if (li) {
	li->nsuffix = _free(li->nsuffix);	/* XXX elements are shared */
	li->filex = _free(li->filex);
    }
    return _free(li);
}

FSM_t newFSM(cpioMapFlags mapflags)
{
    FSM_t fsm = xcalloc(1, sizeof(*fsm));
    fsm->mapFlags = mapflags;
    return fsm;
}

FSM_t freeFSM(FSM_t fsm)
{
    if (fsm) {
	fsm->path = _constfree(fsm->path);
	while ((fsm->li = fsm->links) != NULL) {
	    fsm->links = fsm->li->next;
	    fsm->li->next = NULL;
	    fsm->li = freeHardLink(fsm->li);
	}
	fsm->dnlx = _free(fsm->dnlx);
	fsm->ldn = _free(fsm->ldn);
	fsm->iter = mapFreeIterator(fsm->iter);
    }
    return _free(fsm);
}

int fsmSetup(FSM_t fsm, fileStage goal,
		rpmts ts, rpmte te, rpmfi fi, FD_t cfd,
		rpm_loff_t * archiveSize, char ** failedFile)
{
    int rc, ec = 0;

    fsm->goal = goal;
    if (cfd != NULL) {
	fsm->cfd = fdLink(cfd, RPMDBG_M("persist (fsm)"));
    }
    fsm->cpioPos = 0;
    fsm->iter = mapInitIterator(ts, te, fi);
    fsm->digestalgo = rpmfiDigestAlgo(fi);

    if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	void * ptr;
	fsm->archivePos = 0;
	ptr = rpmtsNotify(ts, te,
		RPMCALLBACK_INST_START, fsm->archivePos, fi->archiveSize);
    }

    fsm->archiveSize = archiveSize;
    if (fsm->archiveSize)
	*fsm->archiveSize = 0;
    fsm->failedFile = failedFile;
    if (fsm->failedFile)
	*fsm->failedFile = NULL;

    memset(fsm->sufbuf, 0, sizeof(fsm->sufbuf));
    if (fsm->goal == FSM_PKGINSTALL) {
	if (ts && rpmtsGetTid(ts) != (rpm_tid_t)-1)
	    sprintf(fsm->sufbuf, ";%08x", (unsigned)rpmtsGetTid(ts));
    }

    ec = fsm->rc = 0;
    rc = fsmUNSAFE(fsm, FSM_CREATE);
    if (rc && !ec) ec = rc;

    rc = fsmUNSAFE(fsm, fsm->goal);
    if (rc && !ec) ec = rc;

    if (fsm->archiveSize && ec == 0)
	*fsm->archiveSize = fsm->cpioPos;

/* FIX: *fsm->failedFile may be NULL */
   return ec;
}

int fsmTeardown(FSM_t fsm)
{
    int rc = fsm->rc;

    if (!rc)
	rc = fsmUNSAFE(fsm, FSM_DESTROY);

    fsm->iter = mapFreeIterator(fsm->iter);
    if (fsm->cfd != NULL) {
	fsm->cfd = fdFree(fsm->cfd, RPMDBG_M("persist (fsm)"));
	fsm->cfd = NULL;
    }
    fsm->failedFile = NULL;
    return rc;
}

static int fsmMapFContext(FSM_t fsm)
{
    rpmts ts = fsmGetTs(fsm);
    struct stat * st;
    st = &fsm->sb;

    /*
     * Find file security context (if not disabled).
     */
    fsm->fcontext = NULL;
    if (ts != NULL && !(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
	security_context_t scon = NULL;

	if (matchpathcon(fsm->path, st->st_mode, &scon) == 0 && scon != NULL) {
	    fsm->fcontext = scon;
	}
    }
    return 0;
}

#if WITH_CAP
static int fsmMapFCaps(FSM_t fsm)
{
    rpmfi fi = fsmGetFi(fsm);
    const char *captxt = rpmfiFCapsIndex(fi, fsm->ix);
    fsm->fcaps = NULL;
    if (captxt && *captxt != '\0') {
	cap_t fcaps = cap_from_text(captxt);
	if (fcaps) {
	   fsm->fcaps = fcaps;
	}
    } 
    return 0;
}
#endif

/**
 * Map next file path and action.
 * @param fsm		file state machine
 */
static int fsmMapPath(FSM_t fsm)
{
    rpmfi fi = fsmGetFi(fsm);	/* XXX const except for fstates */
    int rc = 0;
    int i;

    fsm->osuffix = NULL;
    fsm->nsuffix = NULL;
    fsm->action = FA_UNKNOWN;

    i = fsm->ix;
    if (fi && i >= 0 && i < rpmfiFC(fi)) {
	rpmte te = fsmGetTe(fsm);
	rpmfs fs = rpmteGetFileStates(te);
	/* XXX these should use rpmfiFFlags() etc */
	fsm->action = rpmfsGetAction(fs, i);
	fsm->fflags = rpmfiFFlagsIndex(fi, i);

	/* src rpms have simple base name in payload. */
	fsm->dirName = rpmfiDNIndex(fi, rpmfiDIIndex(fi, i));
	fsm->baseName = rpmfiBNIndex(fi, i);

	switch (fsm->action) {
	case FA_SKIP:
	    break;
	case FA_UNKNOWN:
	    break;

	case FA_COPYOUT:
	    break;
	case FA_COPYIN:
	case FA_CREATE:
	    break;

	case FA_SKIPNSTATE:
	    if (rpmteType(te) == TR_ADDED)
		rpmfsSetState(fs, i, RPMFILE_STATE_NOTINSTALLED);
	    break;

	case FA_SKIPNETSHARED:
	    if (rpmteType(te) == TR_ADDED)
		rpmfsSetState(fs, i, RPMFILE_STATE_NETSHARED);
	    break;

	case FA_SKIPCOLOR:
	    if (rpmteType(te) == TR_ADDED)
		rpmfsSetState(fs, i, RPMFILE_STATE_WRONGCOLOR);
	    break;

	case FA_BACKUP:
	    if (!(fsm->fflags & RPMFILE_GHOST)) /* XXX Don't if %ghost file. */
	    switch (rpmteType(te)) {
	    case TR_ADDED:
		fsm->osuffix = SUFFIX_RPMORIG;
		break;
	    case TR_REMOVED:
		fsm->osuffix = SUFFIX_RPMSAVE;
		break;
	    }
	    break;

	case FA_ALTNAME:
	    assert(rpmteType(te) == TR_ADDED);
	    if (!(fsm->fflags & RPMFILE_GHOST)) /* XXX Don't if %ghost file. */
		fsm->nsuffix = SUFFIX_RPMNEW;
	    break;

	case FA_SAVE:
	    assert(rpmteType(te) == TR_ADDED);
	    if (!(fsm->fflags & RPMFILE_GHOST)) /* XXX Don't if %ghost file. */
		fsm->osuffix = SUFFIX_RPMSAVE;
	    break;
	case FA_ERASE:
#if 0	/* XXX is this a genhdlist fix? */
	    assert(rpmteType(>te) == TR_REMOVED);
#endif
	    /*
	     * XXX TODO: %ghost probably shouldn't be removed, but that changes
	     * legacy rpm behavior.
	     */
	    break;
	default:
	    break;
	}

	if ((fsm->mapFlags & CPIO_MAP_PATH) || fsm->nsuffix) {
	    const struct stat * st = &fsm->sb;
	    fsm->path = _constfree(fsm->path);
	    fsm->path = fsmFsPath(fsm, st, fsm->subdir,
		(fsm->suffix ? fsm->suffix : fsm->nsuffix));
	}
    }
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
	mode_t finalMode = rpmfiFModeIndex(fi, i);
	dev_t finalRdev = rpmfiFRdevIndex(fi, i);
	time_t finalMtime = rpmfiFMtimeIndex(fi, i);
	const char *user = rpmfiFUserIndex(fi, i);
	const char *group = rpmfiFGroupIndex(fi, i);
	uid_t uid = 0;
	gid_t gid = 0;

	if (user && unameToUid(user, &uid)) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rpmlog(RPMLOG_WARNING,
		    _("user %s does not exist - using root\n"), user);
	    finalMode &= ~S_ISUID;      /* turn off suid bit */
	}

	if (group && gnameToGid(group, &gid)) {
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
	    st->st_rdev = finalRdev;
	    st->st_mtime = finalMtime;
	}
	if (fsm->mapFlags & CPIO_MAP_UID)
	    st->st_uid = uid;
	if (fsm->mapFlags & CPIO_MAP_GID)
	    st->st_gid = gid;

	{   rpmts ts = fsmGetTs(fsm);

	    /*
	     * Set file digest (if not disabled).
	     */
	    if (ts != NULL && !(rpmtsFlags(ts) & RPMTRANS_FLAG_NOFILEDIGEST)) {
		fsm->digest = rpmfiFDigestIndex(fi, i, NULL, NULL);
	    } else {
		fsm->digest = NULL;
	    }
	}
    }
    return 0;
}

/** \ingroup payload
 * Create file from payload stream.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int expandRegular(FSM_t fsm)
{
    const struct stat * st = &fsm->sb;
    rpm_loff_t left = st->st_size;
    int rc = 0;

    rc = fsmNext(fsm, FSM_WOPEN);
    if (rc)
	goto exit;

    if (st->st_size > 0 && fsm->digest != NULL)
	fdInitDigest(fsm->wfd, fsm->digestalgo, 0);

    while (left) {

	fsm->wrlen = (left > fsm->wrsize ? fsm->wrsize : left);
	rc = fsmNext(fsm, FSM_DREAD);
	if (rc)
	    goto exit;

	rc = fsmNext(fsm, FSM_WRITE);
	if (rc)
	    goto exit;

	left -= fsm->wrnb;

	/* don't call this with fileSize == fileComplete */
	if (!rc && left)
	    (void) fsmNext(fsm, FSM_NOTIFY);
    }

    if (st->st_size > 0 && fsm->digest) {
	void * digest = NULL;
	int asAscii = (fsm->digest == NULL ? 1 : 0);

	(void) Fflush(fsm->wfd);
	fdFiniDigest(fsm->wfd, fsm->digestalgo, &digest, NULL, asAscii);

	if (digest == NULL) {
	    rc = CPIOERR_DIGEST_MISMATCH;
	    goto exit;
	}

	if (fsm->digest != NULL) {
	    size_t diglen = rpmDigestLength(fsm->digestalgo);
	    if (memcmp(digest, fsm->digest, diglen))
		rc = CPIOERR_DIGEST_MISMATCH;
	} else {
	    rc = CPIOERR_DIGEST_MISMATCH;
	}
	digest = _free(digest);
    }

exit:
    (void) fsmNext(fsm, FSM_WCLOSE);
    return rc;
}

/** \ingroup payload
 * Write next item to payload stream.
 * @param fsm		file state machine data
 * @param writeData	should data be written?
 * @return		0 on success
 */
static int writeFile(FSM_t fsm, int writeData)
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    char * symbuf = NULL;
    rpm_loff_t left;
    int rc;

    st->st_size = (writeData ? ost->st_size : 0);

    if (S_ISDIR(st->st_mode)) {
	st->st_size = 0;
    } else if (S_ISLNK(st->st_mode)) {
	/*
	 * While linux puts the size of a symlink in the st_size field,
	 * I don't think that's a specified standard.
	 */
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
	rc = fsmUNSAFE(fsm, FSM_READLINK);
	if (rc) goto exit;
	st->st_size = fsm->rdnb;
	rstrcat(&symbuf, fsm->rdbuf);	/* XXX save readlink return. */
    }

    if (fsm->mapFlags & CPIO_MAP_ABSOLUTE) {
	char *p = NULL;
	if (fsm->mapFlags & CPIO_MAP_ADDDOT)
		rstrcat(&p, ".");
	rstrscat(&p, fsm->dirName, fsm->baseName, NULL);
	fsm->path = p;
    } else if (fsm->mapFlags & CPIO_MAP_PATH) {
	rpmfi fi = fsmGetFi(fsm);
	fsm->path = xstrdup((fi->apath ? fi->apath[fsm->ix] + fi->striplen : 
					 rpmfiBNIndex(fi, fsm->ix)));
    }

    rc = fsmNext(fsm, FSM_HWRITE);
    _constfree(fsm->path);
    fsm->path = path;
    if (rc) goto exit;

    if (writeData && S_ISREG(st->st_mode)) {
#ifdef HAVE_MMAP
	char * rdbuf = NULL;
	void * mapped = (void *)-1;
	size_t nmapped;
#endif

	rc = fsmNext(fsm, FSM_ROPEN);
	if (rc) goto exit;

	/* XXX unbuffered mmap generates *lots* of fdio debugging */
#ifdef HAVE_MMAP
	nmapped = 0;
	mapped = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, Fileno(fsm->rfd), 0);
	if (mapped != (void *)-1) {
	    rdbuf = fsm->rdbuf;
	    fsm->rdbuf = (char *) mapped;
	    fsm->rdlen = nmapped = st->st_size;
#if defined(MADV_DONTNEED)
	    xx = madvise(mapped, nmapped, MADV_DONTNEED);
#endif
	}
#endif

	left = st->st_size;

	while (left) {
#ifdef HAVE_MMAP
	  if (mapped != (void *)-1) {
	    fsm->rdnb = nmapped;
	  } else
#endif
	  {
	    fsm->rdlen = (left > fsm->rdsize ? fsm->rdsize : left),
	    rc = fsmNext(fsm, FSM_READ);
	    if (rc) goto exit;
	  }

	    /* XXX DWRITE uses rdnb for I/O length. */
	    rc = fsmNext(fsm, FSM_DWRITE);
	    if (rc) goto exit;

	    left -= fsm->wrnb;
	}

#ifdef HAVE_MMAP
	if (mapped != (void *)-1) {
	    xx = msync(mapped, nmapped, MS_ASYNC);
#if defined(MADV_DONTNEED)
	    xx = madvise(mapped, nmapped, MADV_DONTNEED);
#endif
	    xx = munmap(mapped, nmapped);
	    fsm->rdbuf = rdbuf;
	}
#endif

    } else if (writeData && S_ISLNK(st->st_mode)) {
	/* XXX DWRITE uses rdnb for I/O length. */
	strcpy(fsm->rdbuf, symbuf);	/* XXX restore readlink buffer. */
	fsm->rdnb = strlen(symbuf);
	rc = fsmNext(fsm, FSM_DWRITE);
	if (rc) goto exit;
    }

    rc = fsmNext(fsm, FSM_PAD);
    if (rc) goto exit;

    rc = 0;

exit:
    if (fsm->rfd != NULL)
	(void) fsmNext(fsm, FSM_RCLOSE);
    fsm->opath = opath;
    fsm->path = path;
    free(symbuf);
    return rc;
}

/** \ingroup payload
 * Write set of linked files to payload stream.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int writeLinkedFile(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    int ec = 0;
    int rc;
    int i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    for (i = fsm->li->nlink - 1; i >= 0; i--) {

	if (fsm->li->filex[i] < 0) continue;

	fsm->ix = fsm->li->filex[i];
	rc = fsmNext(fsm, FSM_MAP);

	/* Write data after last link. */
	rc = writeFile(fsm, (i == 0));
	if (fsm->failedFile && rc != 0 && *fsm->failedFile == NULL) {
	    ec = rc;
	    *fsm->failedFile = xstrdup(fsm->path);
	}

	fsm->path = _constfree(fsm->path);
	fsm->li->filex[i] = -1;
    }

    fsm->ix = iterIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return ec;
}

/** \ingroup payload
 * Create pending hard links to existing file.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int fsmMakeLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    int ec = 0;
    int rc;
    int i;

    fsm->path = NULL;
    fsm->opath = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    fsm->ix = fsm->li->filex[fsm->li->createdPath];
    rc = fsmNext(fsm, FSM_MAP);
    fsm->opath = fsm->path;
    fsm->path = NULL;
    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->filex[i] < 0) continue;
	if (fsm->li->createdPath == i) continue;

	fsm->ix = fsm->li->filex[i];
	fsm->path = _constfree(fsm->path);
	rc = fsmNext(fsm, FSM_MAP);
	if (XFA_SKIPPING(fsm->action)) continue;

	rc = fsmUNSAFE(fsm, FSM_VERIFY);
	if (!rc) continue;
	if (!(rc == CPIOERR_ENOENT)) break;

	/* XXX link(fsm->opath, fsm->path) */
	rc = fsmNext(fsm, FSM_LINK);
	if (fsm->failedFile && rc != 0 && *fsm->failedFile == NULL) {
	    ec = rc;
	    *fsm->failedFile = xstrdup(fsm->path);
	}

	fsm->li->linksLeft--;
    }
    fsm->path = _constfree(fsm->path);
    fsm->opath = _constfree(fsm->opath);

    fsm->ix = iterIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    fsm->opath = opath;
    return ec;
}

/** \ingroup payload
 * Commit hard linked file set atomically.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int fsmCommitLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    struct stat * st = &fsm->sb;
    int rc = 0;
    nlink_t i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->sb.st_ino == st->st_ino && fsm->li->sb.st_dev == st->st_dev)
	    break;
    }

    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->filex[i] < 0) continue;
	fsm->ix = fsm->li->filex[i];
	rc = fsmNext(fsm, FSM_MAP);
	if (!XFA_SKIPPING(fsm->action))
	    rc = fsmNext(fsm, FSM_COMMIT);
	fsm->path = _constfree(fsm->path);
	fsm->li->filex[i] = -1;
    }

    fsm->ix = iterIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return rc;
}

/**
 * Remove (if created) directories not explicitly included in package.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int fsmRmdirs(FSM_t fsm)
{
    const char * path = fsm->path;
    void * dnli = dnlInitIterator(fsm, 1);
    char * dn = fsm->rdbuf;
    int dc = dnlCount(dnli);
    int rc = 0;

    fsm->path = NULL;
    dn[0] = '\0';
    if (fsm->ldn != NULL && fsm->dnlx != NULL)
    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
	size_t dnlen = strlen(fsm->path);
	char * te;

	dc = dnlIndex(dnli);
	if (fsm->dnlx[dc] < 1 || fsm->dnlx[dc] >= dnlen)
	    continue;

	/* Copy to avoid const on fsm->path. */
	te = stpcpy(dn, fsm->path) - 1;
	fsm->path = dn;

	/* Remove generated directories. */
	do {
	    if (*te == '/') {
		*te = '\0';
		rc = fsmNext(fsm, FSM_RMDIR);
		*te = '/';
	    }
	    if (rc)
		break;
	    te--;
	} while ((te - fsm->path) > fsm->dnlx[dc]);
    }
    dnli = dnlFreeIterator(dnli);

    fsm->path = path;
    return rc;
}

/**
 * Create (if necessary) directories not explicitly included in package.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int fsmMkdirs(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    const char * path = fsm->path;
    mode_t st_mode = st->st_mode;
    void * dnli = dnlInitIterator(fsm, 0);
    char * dn = fsm->rdbuf;
    int dc = dnlCount(dnli);
    int rc = 0;
    int i;
    rpmts ts = fsmGetTs(fsm);
    security_context_t scon = NULL;

    fsm->path = NULL;

    dn[0] = '\0';
    fsm->dnlx = (dc ? xcalloc(dc, sizeof(*fsm->dnlx)) : NULL);
    if (fsm->dnlx != NULL)
    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
	size_t dnlen = strlen(fsm->path);
	char * te;

	dc = dnlIndex(dnli);
	if (dc < 0) continue;
	fsm->dnlx[dc] = dnlen;
	if (dnlen <= 1)
	    continue;

	if (dnlen <= fsm->ldnlen && rstreq(fsm->path, fsm->ldn))
	    continue;

	/* Copy to avoid const on fsm->path. */
	(void) stpcpy(dn, fsm->path);
	fsm->path = dn;

	/* Assume '/' directory exists, "mkdir -p" for others if non-existent */
	for (i = 1, te = dn + 1; *te != '\0'; te++, i++) {
	    if (*te != '/')
		continue;

	    *te = '\0';

	    /* Already validated? */
	    if (i < fsm->ldnlen &&
		(fsm->ldn[i] == '/' || fsm->ldn[i] == '\0') &&
		rstreqn(fsm->path, fsm->ldn, i))
	    {
		*te = '/';
		/* Move pre-existing path marker forward. */
		fsm->dnlx[dc] = (te - dn);
		continue;
	    }

	    /* Validate next component of path. */
	    rc = fsmUNSAFE(fsm, FSM_LSTAT);
	    *te = '/';

	    /* Directory already exists? */
	    if (rc == 0 && S_ISDIR(ost->st_mode)) {
		/* Move pre-existing path marker forward. */
		fsm->dnlx[dc] = (te - dn);
	    } else if (rc == CPIOERR_ENOENT) {
		*te = '\0';
		st->st_mode = S_IFDIR | (_dirPerms & 07777);
		rc = fsmNext(fsm, FSM_MKDIR);
		if (!rc) {
		    /* XXX FIXME? only new dir will have context set. */
		    /* Get file security context from patterns. */
		    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_NOCONTEXTS)) {
			if (matchpathcon(fsm->path, st->st_mode, &scon) == 0 &&
			    scon != NULL) {
            		    fsm->fcontext = scon;
			    rc = fsmNext(fsm, FSM_LSETFCON);
			}
		    }

		    if (fsm->fcontext == NULL)
			rpmlog(RPMLOG_DEBUG,
			    "%s directory created with perms %04o, no context.\n",
			    fsm->path, (unsigned)(st->st_mode & 07777));
		    else {
			rpmlog(RPMLOG_DEBUG,
			    "%s directory created with perms %04o, context %s.\n",
			    fsm->path, (unsigned)(st->st_mode & 07777),
			    fsm->fcontext);
			freecon(fsm->fcontext);
		    }
		    fsm->fcontext = NULL;
		}
		*te = '/';
	    }
	    if (rc)
		break;
	}
	if (rc) break;

	/* Save last validated path. */
/* FIX: ldn/path annotations ? */
	if (fsm->ldnalloc < (dnlen + 1)) {
	    fsm->ldnalloc = dnlen + 100;
	    fsm->ldn = xrealloc(fsm->ldn, fsm->ldnalloc);
	}
	if (fsm->ldn != NULL) {	/* XXX can't happen */
	    strcpy(fsm->ldn, fsm->path);
 	    fsm->ldnlen = dnlen;
	}
    }
    dnli = dnlFreeIterator(dnli);

    fsm->path = path;
    st->st_mode = st_mode;		/* XXX restore st->st_mode */
/* FIX: ldn/path annotations ? */
    return rc;
}

#ifdef	NOTYET
/**
 * Check for file on disk.
 * @param fsm		file state machine data
 * @return		0 on success
 */
static int fsmStat(FSM_t fsm)
{
    int rc = 0;

    if (fsm->path != NULL) {
	int saveernno = errno;
	rc = fsmUNSAFE(fsm, (!(fsm->mapFlags & CPIO_FOLLOW_SYMLINKS)
			? FSM_LSTAT : FSM_STAT));
	if (rc == CPIOERR_ENOENT) {
	    errno = saveerrno;
	    rc = 0;
	    fsm->exists = 0;
	} else if (rc == 0) {
	    fsm->exists = 1;
	}
    } else {
	/* Skip %ghost files on build. */
	fsm->exists = 0;
    }
    return rc;
}
#endif

static const char * rpmteTypeString(rpmte te)
{
    switch(rpmteType(te)) {
    case TR_ADDED:	return " install";
    case TR_REMOVED:	return "   erase";
    default:		return "???";
    }
}

static void removeSBITS(const char *path)
{
    struct stat stb;
    if (lstat(path, &stb) == 0 && S_ISREG(stb.st_mode) && stb.st_nlink > 1) {
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

#define	IS_DEV_LOG(_x)	\
	((_x) != NULL && strlen(_x) >= (sizeof("/dev/log")-1) && \
	rstreqn((_x), "/dev/log", sizeof("/dev/log")-1) && \
	((_x)[sizeof("/dev/log")-1] == '\0' || \
	 (_x)[sizeof("/dev/log")-1] == ';'))

/**
 * File state machine driver.
 * @param fsm		file state machine
 * @param stage		next stage
 * @return		0 on success
 */
static int fsmStage(FSM_t fsm, fileStage stage)
{
#ifdef	UNUSED
    fileStage prevStage = fsm->stage;
    const char * const prev = fileStageString(prevStage);
#endif
    static int modulo = 4;
    const char * const cur = fileStageString(stage);
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    int saveerrno = errno;
    int rc = fsm->rc;
    rpm_loff_t left;

#define	_fafilter(_a)	\
    (!((_a) == FA_CREATE || (_a) == FA_ERASE || (_a) == FA_COPYIN || (_a) == FA_COPYOUT) \
	? fileActionString(_a) : "")

    if (stage & FSM_DEAD) {
	/* do nothing */
    } else if (stage & FSM_INTERNAL) {
	if (_fsm_debug && !(stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		(unsigned)st->st_mode, (int)st->st_nlink,
		(int)st->st_uid, (int)st->st_gid, (int)st->st_size,
		(fsm->path ? fsm->path : ""),
		_fafilter(fsm->action));
    } else {
	fsm->stage = stage;
	if (_fsm_debug || !(stage & FSM_VERBOSE))
	    rpmlog(RPMLOG_DEBUG, "%-8s  %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		(unsigned)st->st_mode, (int)st->st_nlink,
		(int)st->st_uid, (int)st->st_gid, (int)st->st_size,
		(fsm->path ? fsm->path : ""),
		_fafilter(fsm->action));
    }
#undef	_fafilter

    switch (stage) {
    case FSM_UNKNOWN:
	break;
    case FSM_PKGINSTALL:
	while (1) {
	    /* Clean fsm, free'ing memory. Read next archive header. */
	    rc = fsmUNSAFE(fsm, FSM_INIT);

	    /* Exit on end-of-payload. */
	    if (rc == CPIOERR_HDR_TRAILER) {
		rc = 0;
		break;
	    }

	    /* Exit on error. */
	    if (rc) {
		fsm->postpone = 1;
		(void) fsmNext(fsm, FSM_UNDO);
		break;
	    }

	    /* Extract file from archive. */
	    rc = fsmNext(fsm, FSM_PROCESS);
	    if (rc) {
		(void) fsmNext(fsm, FSM_UNDO);
		break;
	    }

	    /* Notify on success. */
	    (void) fsmNext(fsm, FSM_NOTIFY);

	    rc = fsmNext(fsm, FSM_FINI);
	    if (rc) {
		break;
	    }
	}
	break;
    case FSM_PKGERASE:
    case FSM_PKGCOMMIT:
	while (1) {
	    /* Clean fsm, free'ing memory. */
	    rc = fsmUNSAFE(fsm, FSM_INIT);

	    /* Exit on end-of-payload. */
	    if (rc == CPIOERR_HDR_TRAILER) {
		rc = 0;
		break;
	    }

	    /* Rename/erase next item. */
	    if (fsmNext(fsm, FSM_FINI))
		break;
	}
	break;
    case FSM_PKGBUILD:
	while (1) {

	    rc = fsmUNSAFE(fsm, FSM_INIT);

	    /* Exit on end-of-payload. */
	    if (rc == CPIOERR_HDR_TRAILER) {
		rc = 0;
		break;
	    }

	    /* Exit on error. */
	    if (rc) {
		fsm->postpone = 1;
		(void) fsmNext(fsm, FSM_UNDO);
		break;
	    }

	    /* Copy file into archive. */
	    rc = fsmNext(fsm, FSM_PROCESS);
	    if (rc) {
		(void) fsmNext(fsm, FSM_UNDO);
		break;
	    }

	    /* Notify on success. */
	    (void) fsmNext(fsm, FSM_NOTIFY);

	    if (fsmNext(fsm, FSM_FINI))
		break;
	}

	/* Flush partial sets of hard linked files. */
	if (!(fsm->mapFlags & CPIO_ALL_HARDLINKS)) {
	    nlink_t i, nlink;
	    int j;
	    while ((fsm->li = fsm->links) != NULL) {
		fsm->links = fsm->li->next;
		fsm->li->next = NULL;

		/* Re-calculate link count for archive header. */
		for (j = -1, nlink = 0, i = 0; i < fsm->li->nlink; i++) {
		    if (fsm->li->filex[i] < 0)
			continue;
		    nlink++;
		    if (j == -1) j = i;
		}
		/* XXX force the contents out as well. */
		if (j != 0) {
		    fsm->li->filex[0] = fsm->li->filex[j];
		    fsm->li->filex[j] = -1;
		}
		fsm->li->sb.st_nlink = nlink;

		fsm->sb = fsm->li->sb;	/* structure assignment */
		fsm->osb = fsm->sb;	/* structure assignment */

		if (!rc) rc = writeLinkedFile(fsm);

		fsm->li = freeHardLink(fsm->li);
	    }
	}

	if (!rc)
	    rc = fsmNext(fsm, FSM_TRAILER);

	break;
    case FSM_CREATE:
	{   rpmts ts = fsmGetTs(fsm);
#define	_tsmask	(RPMTRANS_FLAG_PKGCOMMIT | RPMTRANS_FLAG_COMMIT)
	    fsm->commit = ((ts && (rpmtsFlags(ts) & _tsmask) &&
			fsm->goal != FSM_PKGCOMMIT) ? 0 : 1);
#undef _tsmask
	}
	fsm->path = _constfree(fsm->path);
	fsm->opath = _constfree(fsm->opath);
	fsm->dnlx = _free(fsm->dnlx);

	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;

	fsm->rdsize = fsm->wrsize = 0;
	fsm->rdbuf = fsm->rdb = _free(fsm->rdb);
	fsm->wrbuf = fsm->wrb = _free(fsm->wrb);
	if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	    fsm->rdsize = 8 * BUFSIZ;
	    fsm->rdbuf = fsm->rdb = xmalloc(fsm->rdsize);
	    fsm->wrsize = 8 * BUFSIZ;
	    fsm->wrbuf = fsm->wrb = xmalloc(fsm->wrsize);
	}

	fsm->mkdirsdone = 0;
	fsm->ix = -1;
	fsm->links = NULL;
	fsm->li = NULL;
	errno = 0;	/* XXX get rid of EBADF */

	/* Detect and create directories not explicitly in package. */
	if (fsm->goal == FSM_PKGINSTALL) {
	    rc = fsmNext(fsm, FSM_MKDIRS);
	    if (!rc) fsm->mkdirsdone = 1;
	}

	break;
    case FSM_INIT:
	fsm->path = _constfree(fsm->path);
	fsm->postpone = 0;
	fsm->diskchecked = fsm->exists = 0;
	fsm->subdir = NULL;
	fsm->suffix = (fsm->sufbuf[0] != '\0' ? fsm->sufbuf : NULL);
	fsm->action = FA_UNKNOWN;
	fsm->osuffix = NULL;
	fsm->nsuffix = NULL;

	if (fsm->goal == FSM_PKGINSTALL) {
	    /* Read next header from payload, checking for end-of-payload. */
	    rc = fsmUNSAFE(fsm, FSM_NEXT);
	}
	if (rc) break;

	/* Identify mapping index. */
	fsm->ix = ((fsm->goal == FSM_PKGINSTALL)
		? mapFind(fsm->iter, fsm->path) : mapNextIterator(fsm->iter));

	/* Detect end-of-loop and/or mapping error. */
	if (fsm->ix < 0) {
	    if (fsm->goal == FSM_PKGINSTALL) {
#if 0
		rpmlog(RPMLOG_WARNING,
		    _("archive file %s was not found in header file list\n"),
			fsm->path);
#endif
		if (fsm->failedFile && *fsm->failedFile == NULL)
		    *fsm->failedFile = xstrdup(fsm->path);
		rc = CPIOERR_UNMAPPED_FILE;
	    } else {
		rc = CPIOERR_HDR_TRAILER;
	    }
	    break;
	}

	/* On non-install, mode must be known so that dirs don't get suffix. */
	if (fsm->goal != FSM_PKGINSTALL) {
	    rpmfi fi = fsmGetFi(fsm);
	    st->st_mode = rpmfiFModeIndex(fi, fsm->ix);
	}

	/* Generate file path. */
	rc = fsmNext(fsm, FSM_MAP);
	if (rc) break;

	/* Perform lstat/stat for disk file. */
#ifdef	NOTYET
	rc = fsmStat(fsm);
#else
	if (fsm->path != NULL &&
	    !(fsm->goal == FSM_PKGINSTALL && S_ISREG(st->st_mode)))
	{
	    rc = fsmUNSAFE(fsm, (!(fsm->mapFlags & CPIO_FOLLOW_SYMLINKS)
			? FSM_LSTAT : FSM_STAT));
	    if (rc == CPIOERR_ENOENT) {
		errno = saveerrno;
		rc = 0;
		fsm->exists = 0;
	    } else if (rc == 0) {
		fsm->exists = 1;
	    }
	} else {
	    /* Skip %ghost files on build. */
	    fsm->exists = 0;
	}
#endif
	fsm->diskchecked = 1;
	if (rc) break;

	/* On non-install, the disk file stat is what's remapped. */
	if (fsm->goal != FSM_PKGINSTALL)
	    *st = *ost;			/* structure assignment */

	/* Remap file perms, owner, and group. */
	rc = fsmMapAttrs(fsm);
	if (rc) break;

	fsm->postpone = XFA_SKIPPING(fsm->action);
	if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	    /* FIX: saveHardLink can modify fsm */
	    if (S_ISREG(st->st_mode) && st->st_nlink > 1)
		fsm->postpone = saveHardLink(fsm);
	}
	break;
    case FSM_PRE:
	break;
    case FSM_MAP:
	rc = fsmMapPath(fsm);
	break;
    case FSM_MKDIRS:
	rc = fsmMkdirs(fsm);
	break;
    case FSM_RMDIRS:
	if (fsm->dnlx)
	    rc = fsmRmdirs(fsm);
	break;
    case FSM_PROCESS:
	if (fsm->postpone) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rc = fsmNext(fsm, FSM_EAT);
	    break;
	}

	if (fsm->goal == FSM_PKGBUILD) {
	    if (fsm->fflags & RPMFILE_GHOST) /* XXX Don't if %ghost file. */
		break;
	    if (S_ISREG(st->st_mode) && st->st_nlink > 1) {
		hardLink_t li, prev;

if (!(fsm->mapFlags & CPIO_ALL_HARDLINKS)) break;
		rc = writeLinkedFile(fsm);
		if (rc) break;	/* W2DO? */

		for (li = fsm->links, prev = NULL; li; prev = li, li = li->next)
		     if (li == fsm->li)
			break;

		if (prev == NULL)
		    fsm->links = fsm->li->next;
		else
		    prev->next = fsm->li->next;
		fsm->li->next = NULL;
		fsm->li = freeHardLink(fsm->li);
	    } else {
		rc = writeFile(fsm, 1);
	    }
	    break;
	}

	if (fsm->goal != FSM_PKGINSTALL)
	    break;

	if (S_ISREG(st->st_mode)) {
	    const char * path = fsm->path;
	    if (fsm->osuffix)
		fsm->path = fsmFsPath(fsm, st, NULL, NULL);
	    rc = fsmUNSAFE(fsm, FSM_VERIFY);

	    if (rc == 0 && fsm->osuffix) {
		const char * opath = fsm->opath;
		fsm->opath = fsm->path;
		fsm->path = fsmFsPath(fsm, st, NULL, fsm->osuffix);
		rc = fsmNext(fsm, FSM_RENAME);
		if (!rc)
		    rpmlog(RPMLOG_WARNING,
			_("%s saved as %s\n"),
				(fsm->opath ? fsm->opath : ""),
				(fsm->path ? fsm->path : ""));
		fsm->path = _constfree(fsm->path);
		fsm->opath = opath;
	    }

	    fsm->path = path;
	    if (!(rc == CPIOERR_ENOENT)) return rc;
	    rc = expandRegular(fsm);
	} else if (S_ISDIR(st->st_mode)) {
	    mode_t st_mode = st->st_mode;
	    rc = fsmUNSAFE(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_ENOENT) {
		st->st_mode &= ~07777; 		/* XXX abuse st->st_mode */
		st->st_mode |=  00700;
		rc = fsmNext(fsm, FSM_MKDIR);
		st->st_mode = st_mode;		/* XXX restore st->st_mode */
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    const char * opath = fsm->opath;

	    if ((st->st_size + 1) > fsm->rdsize) {
		rc = CPIOERR_HDR_SIZE;
		break;
	    }

	    fsm->wrlen = st->st_size;
	    rc = fsmNext(fsm, FSM_DREAD);
	    if (!rc && fsm->rdnb != fsm->wrlen)
		rc = CPIOERR_READ_FAILED;
	    if (rc) break;

	    fsm->wrbuf[st->st_size] = '\0';
	    /* XXX symlink(fsm->opath, fsm->path) */
	    fsm->opath = fsm->wrbuf;
	    rc = fsmUNSAFE(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_ENOENT)
		rc = fsmNext(fsm, FSM_SYMLINK);
	    fsm->opath = opath;		/* XXX restore fsm->path */
	} else if (S_ISFIFO(st->st_mode)) {
	    mode_t st_mode = st->st_mode;
	    /* This mimics cpio S_ISSOCK() behavior but probably isnt' right */
	    rc = fsmUNSAFE(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_ENOENT) {
		st->st_mode = 0000;		/* XXX abuse st->st_mode */
		rc = fsmNext(fsm, FSM_MKFIFO);
		st->st_mode = st_mode;	/* XXX restore st->st_mode */
	    }
	} else if (S_ISCHR(st->st_mode) ||
		   S_ISBLK(st->st_mode) ||
    S_ISSOCK(st->st_mode))
	{
	    rc = fsmUNSAFE(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_ENOENT)
		rc = fsmNext(fsm, FSM_MKNOD);
	} else {
	    /* XXX Special case /dev/log, which shouldn't be packaged anyways */
	    if (!IS_DEV_LOG(fsm->path))
		rc = CPIOERR_UNKNOWN_FILETYPE;
	}
	if (S_ISREG(st->st_mode) && st->st_nlink > 1) {
	    fsm->li->createdPath = fsm->li->linkIndex;
	    rc = fsmMakeLinks(fsm);
	}
	break;
    case FSM_POST:
	break;
    case FSM_MKLINKS:
	rc = fsmMakeLinks(fsm);
	break;
    case FSM_NOTIFY:		/* XXX move from fsm to psm -> tsm */
	if (fsm->goal == FSM_PKGINSTALL || fsm->goal == FSM_PKGBUILD) {
	    rpmts ts = fsmGetTs(fsm);
	    rpmte te = fsmGetTe(fsm);
	    rpmfi fi = fsmGetFi(fsm);
	    void * ptr;
	    if (fsm->cpioPos > fsm->archivePos) {
		fsm->archivePos = fsm->cpioPos;
		ptr = rpmtsNotify(ts, te, RPMCALLBACK_INST_PROGRESS,
			fsm->archivePos, fi->archiveSize);
	    }
	}
	break;
    case FSM_UNDO:
	if (fsm->postpone)
	    break;
	if (fsm->goal == FSM_PKGINSTALL) {
	    /* XXX only erase if temp fn w suffix is in use */
	    if (fsm->sufbuf[0] != '\0')
		(void) fsmNext(fsm,
		    (S_ISDIR(st->st_mode) ? FSM_RMDIR : FSM_UNLINK));

#ifdef	NOTYET	/* XXX remove only dirs just created, not all. */
	    if (fsm->dnlx)
		(void) fsmNext(fsm, FSM_RMDIRS);
#endif
	    errno = saveerrno;
	}
	if (fsm->failedFile && *fsm->failedFile == NULL)
	    *fsm->failedFile = xstrdup(fsm->path);
	break;
    case FSM_FINI:
	if (!fsm->postpone && fsm->commit) {
	    if (fsm->goal == FSM_PKGINSTALL)
		rc = ((S_ISREG(st->st_mode) && st->st_nlink > 1)
			? fsmCommitLinks(fsm) : fsmNext(fsm, FSM_COMMIT));
	    if (fsm->goal == FSM_PKGCOMMIT)
		rc = fsmNext(fsm, FSM_COMMIT);
	    if (fsm->goal == FSM_PKGERASE)
		rc = fsmNext(fsm, FSM_COMMIT);
	}
	fsm->path = _constfree(fsm->path);
	fsm->opath = _constfree(fsm->opath);
	memset(st, 0, sizeof(*st));
	memset(ost, 0, sizeof(*ost));
	break;
    case FSM_COMMIT:
	/* Rename pre-existing modified or unmanaged file. */
	if (fsm->osuffix && fsm->diskchecked &&
	  (fsm->exists || (fsm->goal == FSM_PKGINSTALL && S_ISREG(st->st_mode))))
	{
	    const char * opath = fsm->opath;
	    const char * path = fsm->path;
	    fsm->opath = fsmFsPath(fsm, st, NULL, NULL);
	    fsm->path = fsmFsPath(fsm, st, NULL, fsm->osuffix);
	    rc = fsmNext(fsm, FSM_RENAME);
	    if (!rc) {
		rpmlog(RPMLOG_WARNING, _("%s saved as %s\n"),
				(fsm->opath ? fsm->opath : ""),
				(fsm->path ? fsm->path : ""));
	    }
	    fsm->path = _constfree(fsm->path);
	    fsm->path = path;
	    fsm->opath = _constfree(fsm->opath);
	    fsm->opath = opath;
	}

	/* Remove erased files. */
	if (fsm->goal == FSM_PKGERASE) {
	    if (fsm->action == FA_ERASE) {
		rpmte te = fsmGetTe(fsm);
		if (S_ISDIR(st->st_mode)) {
		    rc = fsmNext(fsm, FSM_RMDIR);
		    if (!rc) break;
		    switch (rc) {
		    case CPIOERR_ENOENT: /* XXX rmdir("/") linux 2.2.x kernel hack */
		    case CPIOERR_ENOTEMPTY:
	/* XXX make sure that build side permits %missingok on directories. */
			if (fsm->fflags & RPMFILE_MISSINGOK)
			    break;

			/* XXX common error message. */
			rpmlog(
			    (strict_erasures ? RPMLOG_ERR : RPMLOG_DEBUG),
			    _("%s rmdir of %s failed: Directory not empty\n"), 
				rpmteTypeString(te), fsm->path);
			break;
		    default:
			rpmlog(
			    (strict_erasures ? RPMLOG_ERR : RPMLOG_DEBUG),
				_("%s rmdir of %s failed: %s\n"),
				rpmteTypeString(te), fsm->path, strerror(errno));
			break;
		    }
		} else {
		    rc = fsmNext(fsm, FSM_UNLINK);
		    if (!rc) break;
		    switch (rc) {
		    case CPIOERR_ENOENT:
			if (fsm->fflags & RPMFILE_MISSINGOK)
			    break;
		    default:
			rpmlog(
			    (strict_erasures ? RPMLOG_ERR : RPMLOG_DEBUG),
				_("%s unlink of %s failed: %s\n"),
				rpmteTypeString(te), fsm->path, strerror(errno));
			break;
		    }
		}
	    }
	    /* XXX Failure to remove is not (yet) cause for failure. */
	    if (!strict_erasures) rc = 0;
	    break;
	}

	/* XXX Special case /dev/log, which shouldn't be packaged anyways */
	if (!S_ISSOCK(st->st_mode) && !IS_DEV_LOG(fsm->path)) {
	    /* Rename temporary to final file name. */
	    if (!S_ISDIR(st->st_mode) &&
		(fsm->subdir || fsm->suffix || fsm->nsuffix))
	    {
		fsm->opath = fsm->path;
		fsm->path = fsmFsPath(fsm, st, NULL, fsm->nsuffix);
		rc = fsmNext(fsm, FSM_RENAME);
		if (!rc && fsm->nsuffix) {
		    char * opath = fsmFsPath(fsm, st, NULL, NULL);
		    rpmlog(RPMLOG_WARNING, _("%s created as %s\n"),
				(opath ? opath : ""),
				(fsm->path ? fsm->path : ""));
		    opath = _free(opath);
		}
		fsm->opath = _constfree(fsm->opath);
	    }
	    /*
	     * Set file security context (if not disabled).
	     */
	    if (!rc && !getuid()) {
		rc = fsmMapFContext(fsm);
		if (!rc) {
		    rc = fsmNext(fsm, FSM_LSETFCON);
		    freecon(fsm->fcontext);	
		}
		fsm->fcontext = NULL;
	    }
	    if (S_ISLNK(st->st_mode)) {
		if (!rc && !getuid())
		    rc = fsmNext(fsm, FSM_LCHOWN);
	    } else {
		if (!rc && !getuid())
		    rc = fsmNext(fsm, FSM_CHOWN);
		if (!rc)
		    rc = fsmNext(fsm, FSM_CHMOD);
		if (!rc) {
		    time_t mtime = st->st_mtime;
		    rpmfi fi = fsmGetFi(fsm);
		    st->st_mtime = rpmfiFMtimeIndex(fi, fsm->ix);
		    rc = fsmNext(fsm, FSM_UTIME);
		    st->st_mtime = mtime;
		}
#if WITH_CAP
		if (!rc && !S_ISDIR(st->st_mode) && !getuid()) {
		    rc = fsmMapFCaps(fsm);
		    if (!rc && fsm->fcaps) {
			rc = fsmNext(fsm, FSM_SETCAP);
			cap_free(fsm->fcaps);
		    }
		    fsm->fcaps = NULL;
		}
#endif /* WITH_CAP */
	    }
	}

	/* Notify on success. */
	if (!rc)		rc = fsmNext(fsm, FSM_NOTIFY);
	else if (fsm->failedFile && *fsm->failedFile == NULL) {
	    /* XXX ick, stripping const */
	    *fsm->failedFile = (char *) fsm->path;
	    fsm->path = NULL;
	}
	break;
    case FSM_DESTROY:
	fsm->path = _constfree(fsm->path);

	/* Check for hard links missing from payload. */
	while ((fsm->li = fsm->links) != NULL) {
	    fsm->links = fsm->li->next;
	    fsm->li->next = NULL;
	    if (fsm->goal == FSM_PKGINSTALL &&
			fsm->commit && fsm->li->linksLeft)
	    {
		for (nlink_t i = 0 ; i < fsm->li->linksLeft; i++) {
		    if (fsm->li->filex[i] < 0)
			continue;
		    rc = CPIOERR_MISSING_HARDLINK;
		    if (fsm->failedFile && *fsm->failedFile == NULL) {
			fsm->ix = fsm->li->filex[i];
			if (!fsmNext(fsm, FSM_MAP)) {
	    		    /* 
 			     * XXX ick, stripping const. Out-of-sync
 			     * hardlinks handled as sub-state, see
 			     * changeset 4062:02b0c237b675
			     */
			    *fsm->failedFile = (char *) fsm->path;
			    fsm->path = NULL;
			}
		    }
		    break;
		}
	    }
	    if (fsm->goal == FSM_PKGBUILD &&
		(fsm->mapFlags & CPIO_ALL_HARDLINKS))
	    {
		rc = CPIOERR_MISSING_HARDLINK;
            }
	    fsm->li = freeHardLink(fsm->li);
	}
	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;
	fsm->rdbuf = fsm->rdb = _free(fsm->rdb);
	fsm->wrbuf = fsm->wrb = _free(fsm->wrb);
	break;
    case FSM_VERIFY:
	if (fsm->diskchecked && !fsm->exists) {
	    rc = CPIOERR_ENOENT;
	    break;
	}
	if (S_ISREG(st->st_mode)) {
	    /*
	     * XXX HP-UX (and other os'es) don't permit unlink on busy
	     * XXX files.
	     */
	    fsm->opath = fsm->path;
	    fsm->path = rstrscat(NULL, fsm->path, "-RPMDELETE", NULL);
	    rc = fsmNext(fsm, FSM_RENAME);
	    if (!rc)
		    (void) fsmNext(fsm, FSM_UNLINK);
	    else
		    rc = CPIOERR_UNLINK_FAILED;
	    _constfree(fsm->path);
	    fsm->path = fsm->opath;
	    fsm->opath = NULL;
	    return (rc ? rc : CPIOERR_ENOENT);	/* XXX HACK */
	    break;
	} else if (S_ISDIR(st->st_mode)) {
	    if (S_ISDIR(ost->st_mode))		return 0;
	    if (S_ISLNK(ost->st_mode)) {
		rc = fsmUNSAFE(fsm, FSM_STAT);
		if (rc == CPIOERR_ENOENT) rc = 0;
		if (rc) break;
		errno = saveerrno;
		if (S_ISDIR(ost->st_mode))	return 0;
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    if (S_ISLNK(ost->st_mode)) {
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
		rc = fsmUNSAFE(fsm, FSM_READLINK);
		errno = saveerrno;
		if (rc) break;
		if (rstreq(fsm->opath, fsm->rdbuf))	return 0;
	    }
	} else if (S_ISFIFO(st->st_mode)) {
	    if (S_ISFIFO(ost->st_mode))		return 0;
	} else if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
	    if ((S_ISCHR(ost->st_mode) || S_ISBLK(ost->st_mode)) &&
		(ost->st_rdev == st->st_rdev))	return 0;
	} else if (S_ISSOCK(st->st_mode)) {
	    if (S_ISSOCK(ost->st_mode))		return 0;
	}
	    /* XXX shouldn't do this with commit/undo. */
	rc = 0;
	if (fsm->stage == FSM_PROCESS) rc = fsmNext(fsm, FSM_UNLINK);
	if (rc == 0)	rc = CPIOERR_ENOENT;
	return (rc ? rc : CPIOERR_ENOENT);	/* XXX HACK */
	break;

    case FSM_UNLINK:
	if (fsm->mapFlags & CPIO_SBIT_CHECK)
	    removeSBITS(fsm->path);
	rc = unlink(fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)
	    rc = (errno == ENOENT ? CPIOERR_ENOENT : CPIOERR_UNLINK_FAILED);
	break;
    case FSM_RENAME:
	if (fsm->mapFlags & CPIO_SBIT_CHECK)
	    removeSBITS(fsm->path);
	rc = rename(fsm->opath, fsm->path);
#if defined(ETXTBSY) && defined(__HPUX__)
	if (rc && errno == ETXTBSY) {
	    char *path = NULL;
	    rstrscat(&path, fsm->path, "-RPMDELETE", NULL);
	    /*
	     * XXX HP-UX (and other os'es) don't permit rename to busy
	     * XXX files.
	     */
	    rc = rename(fsm->path, path);
	    if (!rc) rc = rename(fsm->opath, fsm->path);
	    free(path);
	}
#endif
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_RENAME_FAILED;
	break;
    case FSM_MKDIR:
	rc = mkdir(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (unsigned)(st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKDIR_FAILED;
	break;
    case FSM_RMDIR:
	rc = rmdir(fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)
	    switch (errno) {
	    case ENOENT:        rc = CPIOERR_ENOENT;    break;
	    case ENOTEMPTY:     rc = CPIOERR_ENOTEMPTY; break;
	    default:            rc = CPIOERR_RMDIR_FAILED; break;
	    }
	break;
    case FSM_LSETFCON:
	if (fsm->fcontext == NULL || *fsm->fcontext == '\0'
	 || rstreq(fsm->fcontext, "<<none>>"))
	    break;
	rc = lsetfilecon(fsm->path, (security_context_t)fsm->fcontext);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->path, fsm->fcontext,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0) rc = (errno == EOPNOTSUPP ? 0 : CPIOERR_LSETFCON_FAILED);
	break;
    case FSM_CHOWN:
	rc = chown(fsm->path, st->st_uid, st->st_gid);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, %d) %s\n", cur,
		fsm->path, (int)st->st_uid, (int)st->st_gid,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
	break;
    case FSM_LCHOWN:
#if ! CHOWN_FOLLOWS_SYMLINK
	rc = lchown(fsm->path, st->st_uid, st->st_gid);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, %d) %s\n", cur,
		fsm->path, (int)st->st_uid, (int)st->st_gid,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
#endif
	break;
    case FSM_CHMOD:
	rc = chmod(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (unsigned)(st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHMOD_FAILED;
	break;
    case FSM_UTIME:
	{   struct utimbuf stamp;
	    stamp.actime = st->st_mtime;
	    stamp.modtime = st->st_mtime;
	    rc = utime(fsm->path, &stamp);
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmlog(RPMLOG_DEBUG, " %8s (%s, 0x%x) %s\n", cur,
			fsm->path, (unsigned)st->st_mtime,
			(rc < 0 ? strerror(errno) : ""));
	    if (rc < 0)	rc = CPIOERR_UTIME_FAILED;
	}
	break;
#if WITH_CAP
    case FSM_SETCAP:
	rc = cap_set_file(fsm->path, fsm->fcaps);
	if (rc < 0) {
	    rc = CPIOERR_SETCAP_FAILED;
	}
	break;
#endif /* WITH_CAP */
    case FSM_SYMLINK:
	rc = symlink(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_SYMLINK_FAILED;
	break;
    case FSM_LINK:
	rc = link(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_LINK_FAILED;
	break;
    case FSM_MKFIFO:
	rc = mkfifo(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (unsigned)(st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKFIFO_FAILED;
	break;
    case FSM_MKNOD:
	/* FIX: check S_IFIFO or dev != 0 */
	rc = mknod(fsm->path, (st->st_mode & ~07777), st->st_rdev);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, 0%o, 0x%x) %s\n", cur,
		fsm->path, (unsigned)(st->st_mode & ~07777),
		(unsigned)st->st_rdev,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKNOD_FAILED;
	break;
    case FSM_LSTAT:
	rc = lstat(fsm->path, ost);
	if (_fsm_debug && (stage & FSM_SYSCALL) && rc && errno != ENOENT)
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, ost) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0) {
	    rc = (errno == ENOENT ? CPIOERR_ENOENT : CPIOERR_LSTAT_FAILED);
	    memset(ost, 0, sizeof(*ost));	/* XXX s390x hackery */
	}
	break;
    case FSM_STAT:
	rc = stat(fsm->path, ost);
	if (_fsm_debug && (stage & FSM_SYSCALL) && rc && errno != ENOENT)
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, ost) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0) {
	    rc = (errno == ENOENT ? CPIOERR_ENOENT : CPIOERR_STAT_FAILED);
	    memset(ost, 0, sizeof(*ost));	/* XXX s390x hackery */
	}
	break;
    case FSM_READLINK:
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
	rc = readlink(fsm->path, fsm->rdbuf, fsm->rdsize - 1);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, rdbuf, %d) %s\n", cur,
		fsm->path, (int)(fsm->rdsize -1), (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_READLINK_FAILED;
	else {
	    fsm->rdnb = rc;
	    fsm->rdbuf[fsm->rdnb] = '\0';
	    rc = 0;
	}
	break;
    case FSM_CHROOT:
	break;

    case FSM_NEXT:
	rc = fsmUNSAFE(fsm, FSM_HREAD);
	if (rc) break;
	if (rstreq(fsm->path, CPIO_TRAILER)) { /* Detect end-of-payload. */
	    fsm->path = _constfree(fsm->path);
	    rc = CPIOERR_HDR_TRAILER;
	}
	if (!rc)
	    rc = fsmNext(fsm, FSM_POS);
	break;
    case FSM_EAT:
	for (left = st->st_size; left > 0; left -= fsm->rdnb) {
	    fsm->wrlen = (left > fsm->wrsize ? fsm->wrsize : left);
	    rc = fsmNext(fsm, FSM_DREAD);
	    if (rc)
		break;
	}
	break;
    case FSM_POS:
	left = (modulo - (fsm->cpioPos % modulo)) % modulo;
	if (left) {
	    fsm->wrlen = left;
	    (void) fsmNext(fsm, FSM_DREAD);
	}
	break;
    case FSM_PAD:
	left = (modulo - (fsm->cpioPos % modulo)) % modulo;
	if (left) {
	    memset(fsm->rdbuf, 0, left);
	    /* XXX DWRITE uses rdnb for I/O length. */
	    fsm->rdnb = left;
	    (void) fsmNext(fsm, FSM_DWRITE);
	}
	break;
    case FSM_TRAILER:
	rc = cpioTrailerWrite(fsm);
	break;
    case FSM_HREAD:
	rc = fsmNext(fsm, FSM_POS);
	if (!rc)
	    rc = cpioHeaderRead(fsm, st);	/* Read next payload header. */
	break;
    case FSM_HWRITE:
	rc = cpioHeaderWrite(fsm, st);		/* Write next payload header. */
	break;
    case FSM_DREAD:
	fsm->rdnb = Fread(fsm->wrbuf, sizeof(*fsm->wrbuf), fsm->wrlen, fsm->cfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, cfd)\trdnb %d\n",
		cur, (fsm->wrbuf == fsm->wrb ? "wrbuf" : "mmap"),
		(int)fsm->wrlen, (int)fsm->rdnb);
	if (fsm->rdnb != fsm->wrlen || Ferror(fsm->cfd))
	    rc = CPIOERR_READ_FAILED;
	if (fsm->rdnb > 0)
	    fsm->cpioPos += fsm->rdnb;
	break;
    case FSM_DWRITE:
	fsm->wrnb = Fwrite(fsm->rdbuf, sizeof(*fsm->rdbuf), fsm->rdnb, fsm->cfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, %d, cfd)\twrnb %d\n",
		cur, (fsm->rdbuf == fsm->rdb ? "rdbuf" : "mmap"),
		(int)fsm->rdnb, (int)fsm->wrnb);
	if (fsm->rdnb != fsm->wrnb || Ferror(fsm->cfd))
	    rc = CPIOERR_WRITE_FAILED;
	if (fsm->wrnb > 0)
	    fsm->cpioPos += fsm->wrnb;
	break;

    case FSM_ROPEN:
	fsm->rfd = Fopen(fsm->path, "r.ufdio");
	if (fsm->rfd == NULL || Ferror(fsm->rfd)) {
	    if (fsm->rfd != NULL)	(void) fsmNext(fsm, FSM_RCLOSE);
	    fsm->rfd = NULL;
	    rc = CPIOERR_OPEN_FAILED;
	    break;
	}
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, \"r\") rfd %p rdbuf %p\n", cur,
		fsm->path, fsm->rfd, fsm->rdbuf);
	break;
    case FSM_READ:
	fsm->rdnb = Fread(fsm->rdbuf, sizeof(*fsm->rdbuf), fsm->rdlen, fsm->rfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (rdbuf, %d, rfd)\trdnb %d\n",
		cur, (int)fsm->rdlen, (int)fsm->rdnb);
	if (fsm->rdnb != fsm->rdlen || Ferror(fsm->rfd))
	    rc = CPIOERR_READ_FAILED;
	break;
    case FSM_RCLOSE:
	if (fsm->rfd != NULL) {
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmlog(RPMLOG_DEBUG, " %8s (%p)\n", cur, fsm->rfd);
	    (void) rpmswAdd(rpmtsOp(fsmGetTs(fsm), RPMTS_OP_DIGEST),
			fdOp(fsm->rfd, FDSTAT_DIGEST));
	    (void) Fclose(fsm->rfd);
	    errno = saveerrno;
	}
	fsm->rfd = NULL;
	break;
    case FSM_WOPEN:
	fsm->wfd = Fopen(fsm->path, "w.ufdio");
	if (fsm->wfd == NULL || Ferror(fsm->wfd)) {
	    if (fsm->wfd != NULL)	(void) fsmNext(fsm, FSM_WCLOSE);
	    fsm->wfd = NULL;
	    rc = CPIOERR_OPEN_FAILED;
	}
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (%s, \"w\") wfd %p wrbuf %p\n", cur,
		fsm->path, fsm->wfd, fsm->wrbuf);
	break;
    case FSM_WRITE:
	fsm->wrnb = Fwrite(fsm->wrbuf, sizeof(*fsm->wrbuf), fsm->rdnb, fsm->wfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmlog(RPMLOG_DEBUG, " %8s (wrbuf, %d, wfd)\twrnb %d\n",
		cur, (int)fsm->rdnb, (int)fsm->wrnb);
	if (fsm->rdnb != fsm->wrnb || Ferror(fsm->wfd))
	    rc = CPIOERR_WRITE_FAILED;
	break;
    case FSM_WCLOSE:
	if (fsm->wfd != NULL) {
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmlog(RPMLOG_DEBUG, " %8s (%p)\n", cur, fsm->wfd);
	    (void) rpmswAdd(rpmtsOp(fsmGetTs(fsm), RPMTS_OP_DIGEST),
			fdOp(fsm->wfd, FDSTAT_DIGEST));
	    (void) Fclose(fsm->wfd);
	    errno = saveerrno;
	}
	fsm->wfd = NULL;
	break;

    default:
	break;
    }

    if (!(stage & FSM_INTERNAL)) {
	fsm->rc = (rc == CPIOERR_HDR_TRAILER ? 0 : rc);
    }
    return rc;
}

/**
 * Return formatted string representation of file disposition.
 * @param a		file dispostion
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

/**
 * Return formatted string representation of file stages.
 * @param a		file stage
 * @return		formatted string
 */
static const char * fileStageString(fileStage a) {
    switch(a) {
    case FSM_UNKNOWN:	return "unknown";

    case FSM_PKGINSTALL:return "INSTALL";
    case FSM_PKGERASE:	return "ERASE";
    case FSM_PKGBUILD:	return "BUILD";
    case FSM_PKGCOMMIT:	return "COMMIT";
    case FSM_PKGUNDO:	return "UNDO";

    case FSM_CREATE:	return "create";
    case FSM_INIT:	return "init";
    case FSM_MAP:	return "map";
    case FSM_MKDIRS:	return "mkdirs";
    case FSM_RMDIRS:	return "rmdirs";
    case FSM_PRE:	return "pre";
    case FSM_PROCESS:	return "process";
    case FSM_POST:	return "post";
    case FSM_MKLINKS:	return "mklinks";
    case FSM_NOTIFY:	return "notify";
    case FSM_UNDO:	return "undo";
    case FSM_FINI:	return "fini";
    case FSM_COMMIT:	return "commit";
    case FSM_DESTROY:	return "destroy";
    case FSM_VERIFY:	return "verify";

    case FSM_UNLINK:	return "unlink";
    case FSM_RENAME:	return "rename";
    case FSM_MKDIR:	return "mkdir";
    case FSM_RMDIR:	return "rmdir";
    case FSM_LSETFCON:	return "lsetfcon";
    case FSM_CHOWN:	return "chown";
    case FSM_LCHOWN:	return "lchown";
    case FSM_CHMOD:	return "chmod";
    case FSM_UTIME:	return "utime";
    case FSM_SYMLINK:	return "symlink";
    case FSM_LINK:	return "link";
    case FSM_MKFIFO:	return "mkfifo";
    case FSM_MKNOD:	return "mknod";
    case FSM_LSTAT:	return "lstat";
    case FSM_STAT:	return "stat";
    case FSM_READLINK:	return "readlink";
    case FSM_CHROOT:	return "chroot";
    case FSM_SETCAP:	return "setcap";

    case FSM_NEXT:	return "next";
    case FSM_EAT:	return "eat";
    case FSM_POS:	return "pos";
    case FSM_PAD:	return "pad";
    case FSM_TRAILER:	return "trailer";
    case FSM_HREAD:	return "hread";
    case FSM_HWRITE:	return "hwrite";
    case FSM_DREAD:	return "Fread";
    case FSM_DWRITE:	return "Fwrite";

    case FSM_ROPEN:	return "Fopen";
    case FSM_READ:	return "Fread";
    case FSM_RCLOSE:	return "Fclose";
    case FSM_WOPEN:	return "Fopen";
    case FSM_WRITE:	return "Fwrite";
    case FSM_WCLOSE:	return "Fclose";

    default:		return "???";
    }
}
