/** \ingroup payload rpmio
 * \file lib/cpio.c
 *  Handle cpio payloads within rpm packages.
 *
 * \warning FIXME: We don't translate between cpio and system mode bits! These
 * should both be the same, but really odd things are going to happen if
 * that's not true!
 */

#include "system.h"
#include <rpmlib.h>

#include "rollback.h"
#include "rpmerr.h"
#include "debug.h"

/*@access FD_t@*/
/*@access rpmTransactionSet@*/
/*@access TFI_t@*/
/*@access FSM_t@*/

#define CPIO_NEWC_MAGIC	"070701"
#define CPIO_CRC_MAGIC	"070702"
#define TRAILER		"TRAILER!!!"

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

static /*@null@*/ void * _free(/*@only@*/ /*@null@*/ const void * this) {
    if (this)	free((void *)this);
    return NULL;
}

int _fsm_debug = 1;

/** \ingroup payload
 * Keeps track of the set of all hard links to a file in an archive.
 */
struct hardLink {
/*@dependent@*/ struct hardLink * next;
/*@owned@*/ const char ** files;	/* nlink of these, used by install */
/*@owned@*/ const char ** nsuffix;
/*@owned@*/ const void ** fileMaps;
/*@owned@*/ int * filex;
    dev_t dev;
    ino_t inode;
    int nlink;
    int linksLeft;
    int linkIndex;
    int createdPath;
    const struct stat sb;
};

/** \ingroup payload
 */
enum hardLinkType {
	HARDLINK_INSTALL=1,
	HARDLINK_BUILD
};

/** \ingroup payload
 * Defines a single file to be included in a cpio payload.
 */
struct cpioFileMapping {
/*@dependent@*/ const char * archivePath; /*!< Path to store in cpio archive. */
/*@dependent@*/ const char * dirName;	/*!< Payload file directory. */
/*@dependent@*/ const char * baseName;	/*!< Payload file base name. */
/*@dependent@*/ const char * md5sum;	/*!< File MD5 sum (NULL disables). */
    fileAction action;
    int commit;
    mode_t finalMode;		/*!< Mode of payload file (from header). */
    uid_t finalUid;		/*!< Uid of payload file (from header). */
    gid_t finalGid;		/*!< Gid of payload file (from header). */
    cpioMapFlags mapFlags;
};

/**
 */
struct mapi {
/*@dependent@*/ rpmTransactionSet ts;
/*@dependent@*/ TFI_t fi;
    int isave;
    int i;
    struct cpioFileMapping map;
};

/** \ingroup payload
 * File name and stat information.
 */
struct cpioHeader {
/*@owned@*/ const char * path;
/*@owned@*/ const char * opath;
    FD_t cfd;
/*@owned@*/ void * mapi;
/*@dependent@*/ const void * map;
/*@owned@*/ struct hardLink * links;
/*@dependent@*/ struct hardLink * li;
    unsigned int * archiveSize;
/*@dependent@*/ const char ** failedFile;
/*@owned@*/ short * dnlx;
/*@shared@*/ const char * subdir;
    char subbuf[64];	/* XXX eliminate */
/*@shared@*/ const char * osuffix;
/*@shared@*/ const char * nsuffix;
/*@shared@*/ const char * suffix;
    char sufbuf[64];	/* XXX eliminate */
/*@only@*/ char * ldn;
    int ldnlen;
    int ldnalloc;
    int postpone;
    int statdone;
    int mkdirsdone;
    mode_t dperms;
    mode_t fperms;
    int rc;
    fileAction action;
    fileStage stage;
    struct stat osb;
    struct stat sb;
};

/** \ingroup payload
 * Cpio archive header information.
 * @todo Add support for tar (soon) and ar (eventually) archive formats.
 */
struct cpioCrcPhysicalHeader {
    char magic[6];
    char inode[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devMajor[8];
    char devMinor[8];
    char rdevMajor[8];
    char rdevMinor[8];
    char namesize[8];
    char checksum[8];			/* ignored !! */
};

#define	PHYS_HDR_SIZE	110		/*!< Don't depend on sizeof(struct) */

/**
 */
static int fsmFlags(FSM_t fsm, cpioMapFlags mask) {
    struct mapi * mapi = (struct mapi *) fsm->mapi;
    int rc = 0;
    if (mapi) {
	const struct cpioFileMapping * map = fsm->map;
	rc = (map->mapFlags & mask);
    }
    return rc;
}

/**
 */
static int mapCommit(/*@null@*/ const void * this) {
    const struct cpioFileMapping * map = this;
    return (map ? map->commit : 0);
}

#define	SUFFIX_RPMORIG	".rpmorig"
#define	SUFFIX_RPMSAVE	".rpmsave"
#define	SUFFIX_RPMNEW	".rpmnew"

/**
 */
static /*@only@*/ const char * mapArchivePath(/*@null@*/ const void * this) {
    const struct cpioFileMapping * map = this;
    return (map ? xstrdup(map->archivePath) : NULL);
}

/**
 */
static /*@only@*//*@null@*/ const char * mapFsPath(/*@null@*/ const void * this,
	/*@null@*/ const struct stat * st,
	/*@null@*/ const char * subdir,
	/*@null@*/ const char * suffix)
{
    const struct cpioFileMapping * map = this;
    const char * s = NULL;

    if (map) {
	int nb;
	char * t;
	nb = strlen(map->dirName) +
	    (st && subdir && !S_ISDIR(st->st_mode) ? strlen(subdir) : 0) +
	    (st && suffix && !S_ISDIR(st->st_mode) ? strlen(suffix) : 0) +
	    strlen(map->baseName) + 1;
	s = t = xmalloc(nb);
	t = stpcpy(t, map->dirName);
	if (st && subdir && !S_ISDIR(st->st_mode))
	    t = stpcpy(t, subdir);
	t = stpcpy(t, map->baseName);
	if (st && suffix && !S_ISDIR(st->st_mode))
	    t = stpcpy(t, suffix);
    }
    return s;
}

/**
 */
static mode_t mapFinalMode(/*@null@*/ const void * this) {
    const struct cpioFileMapping * map = this;
    return (map ? map->finalMode : 0);
}

/**
 */
static uid_t mapFinalUid(/*@null@*/ const void * this) {
    const struct cpioFileMapping * map = this;
    return (map ? map->finalUid : 0);
}

/**
 */
static gid_t mapFinalGid(/*@null@*/ const void * this) {
    const struct cpioFileMapping * map = this;
    return (map ? map->finalGid : 0);
}

/**
 */
static /*@observer@*/ const char * const mapMd5sum(/*@null@*/ const void * this)
{
    const struct cpioFileMapping * map = this;
    return (map ? map->md5sum : NULL);
}

/**
 */
static /*@only@*/ /*@null@*/ void * mapLink(const void * this) {
    const struct cpioFileMapping * omap = this;
    struct cpioFileMapping * nmap = NULL;
    if (omap) {
	nmap = xcalloc(1, sizeof(*nmap));
	*nmap = *omap;	/* structure assignment */
    }
    return nmap;
}

/**
 */
static inline /*@null@*/ void * mapFree(/*@only@*//*@null@*/ const void * this) {
    return _free((void *)this);
}

/**
 */
static /*@null@*/ void * mapFreeIterator(/*@only@*//*@null@*/const void * this) {
    return _free((void *)this);
}

/**
 */
static void *
mapInitIterator(/*@kept@*/ const void * this, /*@kept@*/ const void * that)
{
    struct mapi * mapi;
    rpmTransactionSet ts = (void *)this;
    TFI_t fi = (void *)that;

    if (fi == NULL)
	return NULL;
    mapi = xcalloc(1, sizeof(*mapi));
    mapi->ts = ts;
    mapi->fi = fi;
    mapi->isave = mapi->i = 0;

    if (ts && ts->notify) {
	(void)ts->notify(fi->h, RPMCALLBACK_INST_START, 0, fi->archiveSize,
		(fi->ap ? fi->ap->key : NULL), ts->notifyData);
    }

    return mapi;
}

/**
 */
static const void * mapNextIterator(void * this) {
    struct mapi * mapi = this;
    rpmTransactionSet ts = mapi->ts;
    TFI_t fi = mapi->fi;
    struct cpioFileMapping * map = &mapi->map;
    int i;

    do {
	if (!((i = mapi->i) < fi->fc))
	    return NULL;
	mapi->i++;
    } while (fi->actions && XFA_SKIPPING(fi->actions[i]));

    mapi->isave = i;

    /* src rpms have simple base name in payload. */
    map->archivePath = (fi->apath ? fi->apath[i] + fi->striplen : fi->bnl[i]);
    map->dirName = fi->dnl[fi->dil[i]];
    map->baseName = fi->bnl[i];
    map->md5sum = (fi->fmd5s ? fi->fmd5s[i] : NULL);
    map->action = (fi->actions ? fi->actions[i] : FA_UNKNOWN);

#define	_tsmask	(RPMTRANS_FLAG_PKGCOMMIT | RPMTRANS_FLAG_COMMIT)
    map->commit = (ts->transFlags & _tsmask) ? 0 : 1;
#undef _tsmask

    map->finalMode = fi->fmodes[i];
    map->finalUid = (fi->fuids ? fi->fuids[i] : fi->uid); /* XXX chmod u-s */
    map->finalGid = (fi->fgids ? fi->fgids[i] : fi->gid); /* XXX chmod g-s */
    map->mapFlags = (fi->fmapflags ? fi->fmapflags[i] : fi->mapflags);
    return map;
}

int pkgAction(const rpmTransactionSet ts, TFI_t fi, int i, fileStage a)
{
    int nb = (!ts->chrootDone ? strlen(ts->rootDir) : 0);
    char * opath = alloca(nb + fi->dnlmax + fi->bnlmax + 64);
    char * o = (!ts->chrootDone ? stpcpy(opath, ts->rootDir) : opath);
    char * npath = alloca(nb + fi->dnlmax + fi->bnlmax + 64);
    char * n = (!ts->chrootDone ? stpcpy(npath, ts->rootDir) : npath);
    char * ext = NULL;
    int rc = 0;

    switch (fi->actions[i]) {
    case FA_REMOVE:
	break;
    case FA_BACKUP:
	if (fi->type == TR_REMOVED)
	    break;
	/*@fallthrough@*/
    case FA_SKIP:
    case FA_SKIPMULTILIB:
    case FA_UNKNOWN:
    case FA_SKIPNSTATE:
    case FA_SKIPNETSHARED:
    case FA_SAVE:
    case FA_ALTNAME:
    case FA_CREATE:
	return 0;
	/*@notreached@*/ break;
    }

    rpmMessage(RPMMESS_DEBUG, _("   file: %s%s action: %s\n"),
		fi->dnl[fi->dil[i]], fi->bnl[i],
		fileActionString((fi->actions ? fi->actions[i] : FA_UNKNOWN)) );

    switch (fi->actions[i]) {
    case FA_SKIP:
    case FA_SKIPMULTILIB:
    case FA_UNKNOWN:
    case FA_CREATE:
    case FA_SKIPNSTATE:
    case FA_SKIPNETSHARED:
    case FA_ALTNAME:
    case FA_SAVE:
	return 0;
	/*@notreached@*/ break;

    case FA_BACKUP:
	ext = (fi->type == TR_ADDED ? SUFFIX_RPMORIG : SUFFIX_RPMSAVE);
	break;

    case FA_REMOVE:
	assert(fi->type == TR_REMOVED);
	/* Append file name to (possible) root dir. */
	(void) stpcpy( stpcpy(o, fi->dnl[fi->dil[i]]), fi->bnl[i]);
	if (S_ISDIR(fi->fmodes[i])) {
	    rc = rmdir(opath);
	    if (!rc) return rc;
	    switch (errno) {
	    case ENOENT: /* XXX rmdir("/") linux 2.2.x kernel hack */
	    case ENOTEMPTY:
#ifdef	NOTYET
		if (fi->fflags[i] & RPMFILE_MISSINGOK)
		    return 0;
#endif
		rpmError(RPMERR_RMDIR, 
			_("%s: cannot remove %s - directory not empty\n"), 
				fiTypeString(fi), o);
		break;
	    default:
		rpmError(RPMERR_RMDIR,
				_("%s rmdir of %s failed: %s\n"),
				fiTypeString(fi), o, strerror(errno));
		break;
	    }
	    return 1;
	    /*@notreached@*/ break;
	}
	rc = unlink(opath);
	if (!rc) return rc;
	if (errno == ENOENT && (fi->fflags[i] & RPMFILE_MISSINGOK))
	    return 0;
	rpmError(RPMERR_UNLINK, _("%s removal of %s failed: %s\n"),
				fiTypeString(fi), o, strerror(errno));
	return 1;
    }

    if (ext == NULL) return 0;

    /* Append file name to (possible) root dir. */
    (void) stpcpy( stpcpy(o, fi->dnl[fi->dil[i]]), fi->bnl[i]);

    /* XXX TR_REMOVED dinna do this. */
    rc = access(opath, F_OK);
    if (rc != 0) return 0;

    (void) stpcpy( stpcpy(n, o), ext);
    rpmMessage(RPMMESS_WARNING, _("%s: %s saved as %s\n"),
			fiTypeString(fi), o, n);

    rc = rename(opath, npath);
    if (!rc) return rc;

    rpmError(RPMERR_RENAME, _("%s rename of %s to %s failed: %s\n"),
			fiTypeString(fi), o, n, strerror(errno));
    return 1;

}

struct dnli {
/*@dependent@*/ TFI_t fi;
/*@only@*/ /*@null@*/ char * active;
    int reverse;
    int isave;
    int i;
};

/**
 */
static /*@null@*/ void * dnlFreeIterator(/*@only@*/ /*@null@*/ const void * this)
{
    if (this) {
	struct dnli * dnli = (void *)this;
	if (dnli->active) free(dnli->active);
    }
    return _free(this);
}

static int dnlCount(void * this)
{
    struct dnli * dnli = this;
    return (dnli ? dnli->fi->dc : 0);
}

static int dnlIndex(void * this)
{
    struct dnli * dnli = this;
    return (dnli ? dnli->isave : -1);
}

/**
 */
static /*@only@*/ void * dnlInitIterator(/*@null@*/ const void * this,
	int reverse)
{
    const struct mapi * mapi = this;
    struct dnli * dnli;
    TFI_t fi;
    int i, j;

    if (mapi == NULL)
	return NULL;
    fi = mapi->fi;
    dnli = xcalloc(1, sizeof(*dnli));
    dnli->fi = fi;
    dnli->reverse = reverse;
    dnli->i = (reverse ? fi->dc : 0);

    if (fi->dc) {
	dnli->active = xcalloc(fi->dc, sizeof(*dnli->active));

	/* Identify parent directories not skipped. */
	for (i = 0; i < fi->fc; i++)
            if (!XFA_SKIPPING(fi->actions[i])) dnli->active[fi->dil[i]] = 1;

	/* Exclude parent directories that are explicitly included. */
	for (i = 0; i < fi->fc; i++) {
	    int dil, dnlen, bnlen;
	    if (!S_ISDIR(fi->fmodes[i]))
		continue;
	    dil = fi->dil[i];
	    dnlen = strlen(fi->dnl[dil]);
	    bnlen = strlen(fi->bnl[i]);
	    for (j = 0; j < fi->dc; j++) {
		const char * dnl;
		int jlen;
		if (!dnli->active[j] || j == dil) continue;
		dnl = fi->dnl[j];
		jlen = strlen(dnl);
		if (jlen != (dnlen+bnlen+1)) continue;
		if (strncmp(dnl, fi->dnl[dil], dnlen)) continue;
		if (strncmp(dnl+dnlen, fi->bnl[i], bnlen)) continue;
		if (dnl[dnlen+bnlen] != '/' || dnl[dnlen+bnlen+1] != '\0')
		    continue;
		dnli->active[j] = 0;
		break;
	    }
	}
	/* Print only once per package. */
	if (!reverse) {
	    j = 0;
	    for (i = 0; i < fi->dc; i++) {
		if (!dnli->active[i]) continue;
		if (j == 0) {
		    j = 1;
		    rpmMessage(RPMMESS_DEBUG,
	_("========= Directories not explictly included in package:\n"));
		}
		rpmMessage(RPMMESS_DEBUG, _("%9d %s\n"), i, fi->dnl[i]);
	    }
	    if (j)
		rpmMessage(RPMMESS_DEBUG, "=========\n");
	}
    }
    return dnli;
}

/**
 */
static const char * dnlNextIterator(void * this) {
    struct dnli * dnli = this;
    TFI_t fi = dnli->fi;
    int i;

    do {
	i = (!dnli->reverse ? dnli->i++ : --dnli->i);
    } while (i >= 0  && i < dnli->fi->dc && !dnli->active[i]);
    dnli->isave = i;
    return (i >= 0 && i < dnli->fi->dc ? fi->dnl[i] : NULL);
}

/**
 */
static int cpioStrCmp(const void * a, const void * b) {
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

/**
 */
static const void * mapFind(void * this, const char * fsmPath) {
    struct mapi * mapi = this;
    const TFI_t fi = mapi->fi;
    const char ** p;

    p = bsearch(&fsmPath, fi->apath, fi->fc, sizeof(fsmPath), cpioStrCmp);
    if (p == NULL) {
	fprintf(stderr, "*** not mapped %s\n", fsmPath);
	return NULL;
    }
    mapi->i = p - fi->apath;
    return mapNextIterator(this);
}

/**
 * Create and initialize set of hard links.
 * @param st		link stat info
 * @param hltype	type of hard link set to create
 * @return		pointer to set of hard links
 */
static /*@only@*/ struct hardLink * newHardLink(const struct stat * st,
				enum hardLinkType hltype)	/*@*/
{
    struct hardLink * li = xcalloc(1, sizeof(*li));

    li->next = NULL;
    li->nlink = st->st_nlink;
    li->dev = st->st_dev;
    li->inode = st->st_ino;
    li->linkIndex = -1;
    li->createdPath = -1;

    li->filex = xcalloc(st->st_nlink, sizeof(li->filex[0]));
    memset(li->filex, -1, (st->st_nlink * sizeof(li->filex[0])));
    switch (hltype) {
    case HARDLINK_BUILD:
	li->linksLeft = st->st_nlink;
	li->fileMaps = xcalloc(st->st_nlink, sizeof(li->fileMaps[0]));
	li->nsuffix = NULL;
	li->files = NULL;
	break;
    case HARDLINK_INSTALL:
	li->linksLeft = 0;
	li->fileMaps = NULL;
	li->nsuffix = xcalloc(st->st_nlink, sizeof(li->nsuffix[0]));
	li->files = xcalloc(st->st_nlink, sizeof(*li->files));
	break;
    }

    {	struct stat * myst = (struct stat *) &li->sb;
	*myst = *st;	/* structure assignment */
    }

    return li;
}

/**
 * Destroy set of hard links.
 * @param li		set of hard links
 */
static /*@null@*/ void * freeHardLink(/*@only@*/ /*@null@*/ struct hardLink * li)
{
    if (li) {
	if (li->files) {
	    int i;
	    for (i = 0; i < li->nlink; i++) {
		/*@-unqualifiedtrans@*/
		li->files[i] = _free(li->files[i]);
		/*@=unqualifiedtrans@*/
	    }
	}
	li->files = _free(li->files);
	li->nsuffix = _free(li->nsuffix);	/* XXX elements are shared */
	li->fileMaps = _free(li->fileMaps);
	li->filex = _free(li->filex);
    }
    return _free(li);
}

FSM_t newFSM(void) {
    FSM_t fsm = xcalloc(1, sizeof(*fsm));
    return fsm;
}

FSM_t freeFSM(FSM_t fsm)
{
    if (fsm) {
	if (fsm->path)	free((void *)fsm->path);
	while ((fsm->li = fsm->links) != NULL) {
	    fsm->links = fsm->li->next;
	    fsm->li->next = NULL;
	    fsm->li = freeHardLink(fsm->li);
	}
	fsm->dnlx = _free(fsm->dnlx);
	fsm->ldn = _free(fsm->ldn);
	fsm->mapi = mapFreeIterator(fsm->mapi);
    }
    return _free(fsm);
}

/**
 * Read data from payload.
 * @param cfd		payload file handle
 * @retval vbuf		data from read
 * @param amount	no. bytes to read
 * @return		no. bytes read
 */
static inline off_t saferead(FD_t cfd, /*@out@*/void * vbuf, size_t amount)
	/*@modifies cfd, *vbuf @*/
{
    char * buf = vbuf;
    off_t rc = 0;

    while (amount > 0) {
	size_t nb;

	nb = Fread(buf, sizeof(buf[0]), amount, cfd);
	if (nb <= 0)
	    return nb;
	rc += nb;
	if (rc >= amount)
	    break;
	buf += nb;
	amount -= nb;
    }
    return rc;
}

/**
 * Read data from payload and update number of bytes read.
 * @param cfd		payload file handle
 * @retval buf		data from read
 * @param size		no. bytes to read
 * @return		no. bytes read
 */
static inline off_t ourread(FD_t cfd, /*@out@*/void * buf, size_t size)
	/*@modifies cfd, *buf @*/
{
    off_t i = saferead(cfd, buf, size);
    if (i > 0)
	fdSetCpioPos(cfd, fdGetCpioPos(cfd) + i);
    return i;
}

/**
 * Write data to payload.
 * @param cfd		payload file handle
 * @param vbuf		data to write
 * @param amount	no. bytes to write
 * @return		no. bytes written
 */
static inline off_t safewrite(FD_t cfd, const void * vbuf, size_t amount)
	/*@modifies cfd @*/
{
    const char * buf = vbuf;
    off_t rc = 0;

    while (amount > 0) {
	size_t nb;

	nb = Fwrite(buf, sizeof(buf[0]), amount, cfd);
	if (nb <= 0)
	    return nb;
	rc += nb;
	if (rc >= amount)
	    break;
	buf += nb;
	amount -= nb;
    }

    return rc;
}

/**
 * Align output payload handle, padding with zeroes.
 * @param cfd		payload file handle
 * @param modulo	data alignment
 * @return		0 on success, CPIOERR_WRITE_FAILED
 */
static inline int padoutfd(FD_t cfd, size_t * where, int modulo)
	/*@modifies cfd, *where @*/
{
    static int buf[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int amount;

    amount = (modulo - (*where % modulo)) % modulo;
    *where += amount;

    if (safewrite(cfd, buf, amount) != amount)
	return CPIOERR_WRITE_FAILED;
    return 0;
}

/**
 * Convert string to unsigned integer (with buffer size check).
 * @param		input string
 * @retval		address of 1st character not processed
 * @param base		numerical conversion base
 * @param num		max no. of bytes to read
 * @return		converted integer
 */
static int strntoul(const char *str, /*@out@*/char **endptr, int base, int num)
	/*@modifies *endptr @*/
{
    char * buf, * end;
    unsigned long ret;

    buf = alloca(num + 1);
    strncpy(buf, str, num);
    buf[num] = '\0';

    ret = strtoul(buf, &end, base);
    if (*end)
	*endptr = ((char *)str) + (end - buf);	/* XXX discards const */
    else
	*endptr = ((char *)str) + strlen(str);

    return ret;
}

#define GET_NUM_FIELD(phys, log) \
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	if (*end) return CPIOERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	memcpy(phys, space, 8);

/**
 * Process next cpio heasder.
 * @retval fsm		file path and stat info
 * @return		0 on success
 */
static int getNextHeader(FSM_t fsm, struct stat * st)
	/*@modifies fsm->cfd, fsm->path, *st @*/
{
    struct cpioCrcPhysicalHeader physHeader;
    int nameSize;
    char * end;
    int major, minor;

    if (ourread(fsm->cfd, &physHeader, PHYS_HDR_SIZE) != PHYS_HDR_SIZE)
	return CPIOERR_READ_FAILED;

    if (strncmp(CPIO_CRC_MAGIC, physHeader.magic, sizeof(CPIO_CRC_MAGIC)-1) &&
	strncmp(CPIO_NEWC_MAGIC, physHeader.magic, sizeof(CPIO_NEWC_MAGIC)-1))
	return CPIOERR_BAD_MAGIC;

    GET_NUM_FIELD(physHeader.inode, st->st_ino);
    GET_NUM_FIELD(physHeader.mode, st->st_mode);
    GET_NUM_FIELD(physHeader.uid, st->st_uid);
    GET_NUM_FIELD(physHeader.gid, st->st_gid);
    GET_NUM_FIELD(physHeader.nlink, st->st_nlink);
    GET_NUM_FIELD(physHeader.mtime, st->st_mtime);
    GET_NUM_FIELD(physHeader.filesize, st->st_size);

    GET_NUM_FIELD(physHeader.devMajor, major);
    GET_NUM_FIELD(physHeader.devMinor, minor);
    st->st_dev = /*@-shiftsigned@*/ makedev(major, minor) /*@=shiftsigned@*/ ;

    GET_NUM_FIELD(physHeader.rdevMajor, major);
    GET_NUM_FIELD(physHeader.rdevMinor, minor);
    st->st_rdev = /*@-shiftsigned@*/ makedev(major, minor) /*@=shiftsigned@*/ ;

    GET_NUM_FIELD(physHeader.namesize, nameSize);

    {	char * t = xmalloc(nameSize + 1);
	if (ourread(fsm->cfd, t, nameSize) != nameSize) {
	    free(t);
	    fsm->path = NULL;
	    return CPIOERR_BAD_HEADER;
	}
	fsm->path = t;
    }

    return 0;
}

/**
 * Create file from payload stream.
 * @todo Legacy: support brokenEndian MD5 checks?
 * @param fsm		file path and stat info
 * @return		0 on success
 */
static int expandRegular(FSM_t fsm)
		/*@modifies fileSystem, fsm->cfd @*/
{
    const char * filemd5;
    FD_t ofd;
    char buf[BUFSIZ];
    int bytesRead;
    const struct stat * st = &fsm->sb;
    int left = st->st_size;
    int rc = 0;

/* XXX HACK_ALERT: something fubar with linked files. */
    filemd5 = (st->st_nlink == 1 ? mapMd5sum(fsm->map) : NULL);

    {
	const char * opath = fsm->opath;
	const char * path = fsm->path;

	if (fsm->osuffix)
	    fsm->path = mapFsPath(fsm->map, st, NULL, NULL);
	rc = fsmStage(fsm, FSM_VERIFY);
	if (rc == 0 && fsm->osuffix) {
	    fsm->opath = fsm->path;
	    fsm->path = mapFsPath(fsm->map, st, NULL, fsm->osuffix);
	    rc = fsmStage(fsm, FSM_RENAME);
if (!rc)
rpmMessage(RPMMESS_WARNING, _("%s saved as %s\n"), fsm->opath, fsm->path);
	    fsm->path = _free(fsm->path);
	    fsm->opath = _free(fsm->opath);
	}

	fsm->path = path;
	fsm->opath = opath;
    }

    if (rc != CPIOERR_LSTAT_FAILED) return rc;
    rc = 0;

    ofd = Fopen(fsm->path, "w.ufdio");
    if (ofd == NULL || Ferror(ofd))
	return CPIOERR_OPEN_FAILED;

    /* XXX This doesn't support brokenEndian checks. */
    if (filemd5)
	fdInitMD5(ofd, 0);

    while (left) {
	bytesRead = ourread(fsm->cfd, buf, left < sizeof(buf) ? left : sizeof(buf));
	if (bytesRead <= 0) {
	    rc = CPIOERR_READ_FAILED;
	    break;
	}

	if (Fwrite(buf, sizeof(buf[0]), bytesRead, ofd) != bytesRead) {
	    rc = CPIOERR_COPY_FAILED;
	    break;
	}

	left -= bytesRead;

	/* don't call this with fileSize == fileComplete */
	if (!rc && left)
	    (void) fsmStage(fsm, FSM_NOTIFY);
    }

    if (filemd5) {
	const char * md5sum = NULL;

	Fflush(ofd);
	fdFiniMD5(ofd, (void **)&md5sum, NULL, 1);

	if (md5sum == NULL) {
	    rc = CPIOERR_MD5SUM_MISMATCH;
	} else {
	    if (strcmp(md5sum, filemd5))
		rc = CPIOERR_MD5SUM_MISMATCH;
	    md5sum = _free(md5sum);
	}
    }

    Fclose(ofd);

    return rc;
}

static int fsmMakeLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    int rc = 0;
    int i;

    fsm->opath = fsm->li->files[fsm->li->createdPath];
    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->filex[i] < 0) continue;
	if (i == fsm->li->createdPath) continue;
	if (fsm->li->files[i] == NULL) continue;

	fsm->path = fsm->li->files[i];
	rc = fsmStage(fsm, FSM_VERIFY);
	if (!rc) continue;
	if (rc != CPIOERR_LSTAT_FAILED) break;

	/* XXX link(fsm->opath, fsm->path) */
	rc = fsmStage(fsm, FSM_LINK);
	if (rc) break;

#ifdef	DYING
	/*@-unqualifiedtrans@*/
	fsm->li->files[i] = _free(fsm->li->files[i]);
	/*@=unqualifiedtrans@*/
#endif
	fsm->li->linksLeft--;
    }

    if (rc && fsm->failedFile && *fsm->failedFile == NULL)
	*fsm->failedFile = xstrdup(fsm->path);

    fsm->path = path;
    fsm->opath = opath;

    return rc;
}

static int fsmCommitLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    struct stat * st = &fsm->sb;
    struct mapi * mapi = fsm->mapi;
    int mapiIndex = mapi->i;
    int rc = 0;
    int i;

    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->inode == st->st_ino && fsm->li->dev == st->st_dev)
	    break;
    }

    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->files[i] == NULL) continue;
	mapi->i = fsm->li->filex[i];
	fsm->path = xstrdup(fsm->li->files[i]);
	rc = fsmStage(fsm, FSM_FINALIZE);
	fsm->path = _free(fsm->path);
	fsm->li->filex[i] = -1;
    }

    mapi->i = mapiIndex;
    fsm->nsuffix = nsuffix;
    fsm->path = path;
    return rc;
}

int fsmStage(FSM_t fsm, fileStage stage)
{
#ifdef	UNUSED
    fileStage prevStage = fsm->stage;
    const char * const prev = fileStageString(prevStage);
#endif
    const char * const cur = fileStageString(stage);
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    int saveerrno = errno;
    int rc = fsm->rc;
    int i;

    if (stage & FSM_INTERNAL) {
	if (_fsm_debug && !(stage == FSM_VERIFY || stage == FSM_NOTIFY))
	    rpmMessage(RPMMESS_DEBUG, " %8s %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		st->st_mode, st->st_nlink, st->st_uid, st->st_gid, st->st_size,
		(fsm->path ? fsm->path : ""),
		((fsm->action != FA_UNKNOWN && fsm->action != FA_CREATE)
			? fileActionString(fsm->action) : ""));
    } else {
	fsm->stage = stage;
	if (_fsm_debug || !(stage & FSM_QUIET))
	    rpmMessage(RPMMESS_DEBUG, "%-8s  %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		st->st_mode, st->st_nlink, st->st_uid, st->st_gid, st->st_size,
		(fsm->path ? fsm->path : ""),
		((fsm->action != FA_UNKNOWN && fsm->action != FA_CREATE)
			? fileActionString(fsm->action) : ""));
    }

    switch (stage) {
    case FSM_UNKNOWN:
	break;
    case FSM_INSTALL:
	break;
    case FSM_ERASE:
	break;
    case FSM_BUILD:
	break;
    case FSM_CREATE:
	fsm->path = NULL;
	fsm->dnlx = _free(fsm->dnlx);
	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;
	fsm->mkdirsdone = 0;
	fsm->map = NULL;
	fsm->links = NULL;
	fsm->li = NULL;
	errno = 0;	/* XXX get rid of EBADF */

#ifdef	NOTYET
	rc = fsmStage(fsm, FSM_MKDIRS);
#endif

	break;
    case FSM_INIT:
	fsm->path = _free(fsm->path);
	fsm->postpone = 0;
	fsm->statdone = 0;
	fsm->dperms = 0755;
	fsm->fperms = 0644;
	fsm->subdir = NULL;
	fsm->suffix = (fsm->sufbuf[0] != '\0' ? fsm->sufbuf : NULL);
	fsm->action = FA_UNKNOWN;
	fsm->osuffix = NULL;
	fsm->nsuffix = NULL;

	if (fsm->cfd) {
	    /* Read next header from payload. */
	    rc = fsmStage(fsm, FSM_POS);
	    if (!rc)
		rc = fsmStage(fsm, FSM_NEXT);
	}

	/* Remap mode/uid/gid/name of file from archive. */
	if (!rc)
	    rc = fsmStage(fsm, FSM_MAP);

	    /* FSM_INIT -> {FSM_PRE,FSM_DESTROY} */
	break;
    case FSM_MAP:
	/* No map, nothing to do. */
	/* @todo FA_ALTNAME rename still needs doing. */
	if (fsm->mapi == NULL) break;
	fsm->map = mapFind(fsm->mapi, fsm->path);
	fsm->action = fsmAction(fsm, &fsm->osuffix, &fsm->nsuffix);

	if (fsmFlags(fsm, CPIO_MAP_PATH) || fsm->nsuffix) {
	    fsm->path = _free(fsm->path);
	    fsm->path = mapFsPath(fsm->map, st, fsm->subdir,
		(fsm->suffix ? fsm->suffix : fsm->nsuffix));
	}

	if (fsmFlags(fsm, CPIO_MAP_MODE))
	    st->st_mode = mapFinalMode(fsm->map);
	if (fsmFlags(fsm,  CPIO_MAP_UID))
	    st->st_uid = mapFinalUid(fsm->map);
	if (fsmFlags(fsm, CPIO_MAP_GID))
	    st->st_gid = mapFinalGid(fsm->map);
	break;
    case FSM_MKDIRS:
	{   const char * path = fsm->path;
	    mode_t st_mode = st->st_mode;
	    void * dnli = dnlInitIterator(fsm->mapi, 0);
#ifdef DYING
	    char dn[BUFSIZ];		/* XXX add to fsm */
#else
	    char * dn = xcalloc(1, BUFSIZ);
#endif
	    int dc = dnlCount(dnli);
	    int i;

	    dn[0] = '\0';
	    fsm->dnlx = (dc ? xcalloc(dc, sizeof(*fsm->dnlx)) : NULL);
	    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
		int dnlen = strlen(fsm->path);
		char * te;

		dc = dnlIndex(dnli);
		fsm->dnlx[dc] = dnlen;
		if (dnlen <= 1)
		    continue;
		if (dnlen <= fsm->ldnlen && !strcmp(fsm->path, fsm->ldn))
		    continue;

		/* Copy to avoid const on fsm->path. */
		(void) stpcpy(dn, fsm->path);
		fsm->path = dn;

		/* Initial mode for created dirs is 0700 */
		st->st_mode &= ~07777; 		/* XXX abuse st->st_mode */
		st->st_mode |=  00700;

		/* Assume '/' directory, otherwise "mkdir -p" */
		for (i = 1, te = dn + 1; *te; te++, i++) {
		    if (*te != '/') continue;

		    *te = '\0';

		    /* Already validated? */
		    if (i < fsm->ldnlen &&
			(fsm->ldn[i] == '/' || fsm->ldn[i] == '\0') &&
			!strncmp(fsm->path, fsm->ldn, i))
		    {
			*te = '/';
			/* Move pre-existing path marker forward. */
			fsm->dnlx[dc] = (te - dn);
			continue;
		    }

		    /* Validate next component of path. */
		    rc = fsmStage(fsm, FSM_LSTAT);
		    *te = '/';

		    /* Directory already exists? */
		    if (rc == 0 && S_ISDIR(ost->st_mode)) {
			/* Move pre-existing path marker forward. */
			fsm->dnlx[dc] = (te - dn);
		    } else if (rc == CPIOERR_LSTAT_FAILED) {
			*te = '\0';
			rc = fsmStage(fsm, FSM_MKDIR);
			*te = '/';
		    }
		    if (rc) break;
		}
		if (rc) break;

		/* Save last validated path. */
		if (fsm->ldnalloc < (dnlen + 1)) {
		    fsm->ldnalloc = dnlen + 100;
		    fsm->ldn = xrealloc(fsm->ldn, fsm->ldnalloc);
		}
		strcpy(fsm->ldn, fsm->path);
	 	fsm->ldnlen = dnlen;
	    }
	    dnli = dnlFreeIterator(dnli);
	    free(dn);
	    fsm->path = path;
	    st->st_mode = st_mode;		/* XXX restore st->st_mode */
	}
	break;
    case FSM_RMDIRS:
	if (fsm->dnlx) {
	    const char * path = fsm->path;
	    void * dnli = dnlInitIterator(fsm->mapi, 1);
#ifdef	DYING
	    char dn[BUFSIZ];		/* XXX add to fsm */
#else
	    char * dn = calloc(1, BUFSIZ);
#endif
	    int dc = dnlCount(dnli);

	    dn[0] = '\0';
	    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
		int dnlen = strlen(fsm->path);
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
			rc = fsmStage(fsm, FSM_RMDIR);
			*te = '/';
		    }
		    if (rc) break;
		    te--;
		} while ((te - dn) > fsm->dnlx[dc]);
	    }
	    dnli = dnlFreeIterator(dnli);
	    free(dn);
	    fsm->path = path;
	}
	break;
    case FSM_PRE:
	if (S_ISREG(st->st_mode) && st->st_nlink > 1) {
	    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
		if (fsm->li->inode == st->st_ino && fsm->li->dev == st->st_dev)
		    break;
	    }

	    /* Save new hard link set. */
	    if (fsm->li == NULL) {
		fsm->li = newHardLink(st, HARDLINK_INSTALL);
		fsm->li->next = fsm->links;
		fsm->links = fsm->li;
	    }

	    fsm->li->filex[fsm->li->linksLeft] = fsmGetIndex(fsm);
	    fsm->li->files[fsm->li->linksLeft] = xstrdup(fsm->path);
	    /* XXX this is just an observer pointer. */
	    fsm->li->nsuffix[fsm->li->linksLeft] = fsm->nsuffix;
fprintf(stderr, "*** %p link[%d:%d] %d filex %d %s\n", fsm->li, fsm->li->linksLeft, st->st_nlink, (int)st->st_size, fsm->li->filex[fsm->li->linksLeft], fsm->li->files[fsm->li->linksLeft]);
	    fsm->li->linksLeft++;

	    if (!(st->st_size || fsm->li->linksLeft == st->st_nlink)) {
		fsm->postpone = 1;
	    } else {
		TFI_t fi = fsmGetFi(fsm);
		int j;

		/* Here come the bits, time to choose a non-skipped file name. */
		for (j = fsm->li->linksLeft - 1; j >= 0; j--) {
		    i = fsm->li->filex[j];
		    if (i < 0 || XFA_SKIPPING(fi->actions[i]))
			continue;
		    break;
		}
		/* Are all links skipped or not encountered yet? */
		if (j < 0) {
		    /* XXX W2DO? */
		    fsm->postpone = 1;
		} else {
		    fsm->li->linkIndex = j;
		    fsm->path = _free(fsm->path);
		    fsm->path = xstrdup(fsm->li->files[j]);
		}
	    }
	}

	else {
#ifndef	DYING
	    /* @todo This makes all dir paths, only needs doing once. */
	    if (!fsm->mkdirsdone) {
		rc = fsmStage(fsm, FSM_MKDIRS);
		fsm->mkdirsdone = 1;
	    }
#endif
	    fsm->postpone = XFA_SKIPPING(fsm->action);
	}

	if (fsm->postpone) {
	    if (fsm->cfd) {
		(void) fsmStage(fsm, FSM_POS);
		(void) fsmStage(fsm, FSM_EAT);
	    }
	    /* FSM_PRE -> FSM_INIT */
	}
	if (fsm->cfd)
	    (void) fsmStage(fsm, FSM_POS);
	    /* FSM_PRE -> FSM_PROCESS */
	break;
    case FSM_PROCESS:
	if (fsm->postpone)
	    break;
	if (S_ISREG(st->st_mode)) {
	    /* XXX nlink > 1, insure that path/createdPath is non-skipped. */
	    rc = expandRegular(fsm);
	} else if (S_ISDIR(st->st_mode)) {
	    mode_t st_mode = st->st_mode;
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED) {
		st->st_mode &= ~07777; 		/* XXX abuse st->st_mode */
		st->st_mode |=  00700;
		rc = fsmStage(fsm, FSM_MKDIR);
		st->st_mode = st_mode;		/* XXX restore st->st_mode */
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    const char * opath = fsm->opath;
	    char buf[2048];			/* XXX add to fsm */

	    if ((st->st_size + 1)> sizeof(buf)) {
		rc = CPIOERR_HDR_SIZE;
		break;
	    }
	    if (ourread(fsm->cfd, buf, st->st_size) != st->st_size) {
		rc = CPIOERR_READ_FAILED;
		break;
	    }
	    buf[st->st_size] = '\0';

	    /* XXX symlink(fsm->opath, fsm->path) */
	    fsm->opath = buf;		/* XXX abuse fsm->path */
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED)
		rc = fsmStage(fsm, FSM_SYMLINK);
	    fsm->opath = opath;		/* XXX restore fsm->path */
	} else if (S_ISFIFO(st->st_mode) || S_ISSOCK(st->st_mode)) {
	    mode_t st_mode = st->st_mode;
	    /* This mimics cpio S_ISSOCK() behavior but probably isnt' right */
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED) {
		st->st_mode = 0000;		/* XXX abuse st->st_mode */
		rc = fsmStage(fsm, FSM_MKFIFO);
		st->st_mode = st_mode;	/* XXX restore st->st_mode*/
	    }
	} else if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == CPIOERR_LSTAT_FAILED)
		rc = fsmStage(fsm, FSM_MKNOD);
	} else {
	    rc = CPIOERR_UNKNOWN_FILETYPE;
	}
	    /* FSM_PROCESS -> FSM_POST */
	break;
    case FSM_POST:
	if (fsm->postpone)
	    break;
	/* XXX nlink > 1, insure that path/createdPath is non-skipped. */
	if (S_ISREG(st->st_mode) && st->st_nlink > 1) {
	    fsm->li->createdPath = fsm->li->linkIndex;
	    rc = fsmStage(fsm, FSM_MKLINKS);
	}
	    /* FSM_POST -> {FSM_COMMIT,FSM_UNDO} */
	break;
    case FSM_MKLINKS:
	rc = fsmMakeLinks(fsm);
	break;
    case FSM_NOTIFY:		/* XXX move from fsm to psm -> tsm */
	{   rpmTransactionSet ts = fsmGetTs(fsm);
	    TFI_t fi = fsmGetFi(fsm);
	    if (ts && ts->notify && fi)
		(void)ts->notify(fi->h, RPMCALLBACK_INST_PROGRESS,
			fdGetCpioPos(fsm->cfd), fi->archiveSize,
			(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
	break;
    case FSM_UNDO:
	if (fsm->postpone)
	    break;
	if (S_ISDIR(st->st_mode)) {
	    (void) fsmStage(fsm, FSM_RMDIR);
	} else {
	    (void) fsmStage(fsm, FSM_UNLINK);
	}
#ifdef	NOTYET	/* XXX remove only dirs just created, not all. */
	if (fsm->dnlx)
	    (void) fsmStage(fsm, FSM_RMDIRS);
#endif
	errno = saveerrno;
	if (fsm->failedFile && *fsm->failedFile == NULL)
	    *fsm->failedFile = xstrdup(fsm->path);
	    /* FSM_UNDO -> FSM_INIT */
	break;
    case FSM_COMMIT:
	if (!fsm->postpone && mapCommit(fsm->map)) {
	    if (S_ISREG(st->st_mode) && st->st_nlink > 1) {
		rc = fsmCommitLinks(fsm);
	    } else {
		rc = fsmStage(fsm, FSM_FINALIZE);
	    }
	}
	fsm->path = _free(fsm->path);
	memset(st, 0, sizeof(*st));
	    /* FSM_COMMIT -> FSM_INIT */
	break;
    case FSM_FINALIZE:
	if (!S_ISDIR(st->st_mode) && (fsm->subdir || fsm->suffix)) {
	    fsm->opath = fsm->path;
	    fsm->path = mapFsPath(fsm->map, st, NULL, fsm->nsuffix);
	    rc = fsmStage(fsm, FSM_RENAME);
if (!rc && fsm->nsuffix) {
const char * opath = mapFsPath(fsm->map, st, NULL, NULL);
rpmMessage(RPMMESS_WARNING, _("%s created as %s\n"), opath, fsm->path);
opath = _free(opath);
}
	    fsm->opath = _free(fsm->opath);
	}
	if (S_ISLNK(st->st_mode)) {
	    if (!rc && !getuid() &&
			!(fsm->statdone && st->st_mode == ost->st_mode))
		rc = fsmStage(fsm, FSM_LCHOWN);
	} else {
	    if (!rc && !getuid() &&
			!(fsm->statdone && st->st_uid == ost->st_uid
					&& st->st_uid == ost->st_uid))
		rc = fsmStage(fsm, FSM_CHOWN);
	    if (!rc &&
			!(fsm->statdone &&
				(st->st_mode & 07777) == (ost->st_mode & 07777)))
		rc = fsmStage(fsm, FSM_CHMOD);
	    if (!rc)
		rc = fsmStage(fsm, FSM_UTIME);
	}
	/* Notify on success. */
	if (!rc)		rc = fsmStage(fsm, FSM_NOTIFY);
	break;
    case FSM_DESTROY:
	fsm->path = _free(fsm->path);
	/* Create any remaining links (if no error), and clean up. */
	while ((fsm->li = fsm->links) != NULL) {
	    fsm->links = fsm->li->next;
	    fsm->li->next = NULL;
	    if (fsm->li->linksLeft) {
		for (i = 0 ; i < fsm->li->linksLeft; i++) {
		    if (fsm->li->filex[i] < 0) continue;
		    rc = CPIOERR_MISSING_HARDLINK;
		    if (!(fsm->li->files && fsm->li->files[i])) continue;
		    if (fsm->failedFile && *fsm->failedFile == NULL)
			*fsm->failedFile = xstrdup(fsm->li->files[i]);
		    break;
		}
	    }
	    fsm->li = freeHardLink(fsm->li);
	}
	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;
	break;
    case FSM_VERIFY:
	rc = lstat(fsm->path, ost);
	if (rc < 0 && errno == ENOENT)
	    errno = saveerrno;
	if (rc < 0) return CPIOERR_LSTAT_FAILED;
	fsm->statdone = 1;

	if (S_ISREG(st->st_mode)) {
	    char * path = alloca(strlen(fsm->path) + sizeof("-RPMDELETE"));
	    (void) stpcpy( stpcpy(path, fsm->path), "-RPMDELETE");
	    /*
	     * XXX HP-UX (and other os'es) don't permit unlink on busy
	     * XXX files.
	     */
	    fsm->opath = fsm->path;
	    fsm->path = path;
	    rc = fsmStage(fsm, FSM_RENAME);
	    if (!rc)
		    (void) fsmStage(fsm, FSM_UNLINK);
	    else
		    rc = CPIOERR_UNLINK_FAILED;
	    fsm->path = fsm->opath;
	    fsm->opath = NULL;
	    return (rc ? rc : CPIOERR_LSTAT_FAILED);
	    break;
	} else if (S_ISDIR(st->st_mode)) {
	    if (S_ISDIR(ost->st_mode))		return 0;
	    if (S_ISLNK(ost->st_mode)) {
		fsm->statdone = 0;
		rc = stat(fsm->path, ost);
		if (rc < 0 && errno != ENOENT)
		    return CPIOERR_STAT_FAILED;
		errno = saveerrno;
		if (S_ISDIR(ost->st_mode))	return 0;
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    if (S_ISLNK(ost->st_mode)) {
		char buf[2048];
		rc = readlink(fsm->path, buf, sizeof(buf) - 1);
		errno = saveerrno;
		if (rc > 0) {
		    buf[rc] = '\0';
		    if (!strcmp(fsm->opath, buf))	return 0;
		}
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
	if (fsm->stage == FSM_PROCESS) rc = fsmStage(fsm, FSM_UNLINK);
	if (rc == 0)	rc = CPIOERR_LSTAT_FAILED;
	return (rc ? rc : CPIOERR_LSTAT_FAILED);	/* XXX HACK */
	break;
    case FSM_UNLINK:
	rc = unlink(fsm->path);
if (rc < 0) fprintf(stderr, "*** %s(%s) %m\n", cur, fsm->path);
	if (rc < 0)	rc = CPIOERR_UNLINK_FAILED;
	break;
    case FSM_RENAME:
	rc = rename(fsm->opath, fsm->path);
if (rc < 0) fprintf(stderr, "*** %s(%s,%s) %m\n", cur, fsm->opath, fsm->path);
	if (rc < 0)	rc = CPIOERR_RENAME_FAILED;
	break;
    case FSM_MKDIR:
	rc = mkdir(fsm->path, (st->st_mode & 07777));
if (rc < 0) fprintf(stderr, "*** %s(%s,%o) %m\n", cur, fsm->path, (st->st_mode & 07777));
	if (rc < 0)	rc = CPIOERR_MKDIR_FAILED;
	break;
    case FSM_RMDIR:
	rc = rmdir(fsm->path);
if (rc < 0) fprintf(stderr, "*** %s(%s) %m\n", cur, fsm->path);
	if (rc < 0)	rc = CPIOERR_RMDIR_FAILED;
	break;
    case FSM_CHOWN:
	rc = chown(fsm->path, st->st_uid, st->st_gid);
if (rc < 0) fprintf(stderr, "*** %s(%s,%d,%d) %m\n", cur, fsm->path, st->st_uid, st->st_gid);
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
	break;
    case FSM_LCHOWN:
#if ! CHOWN_FOLLOWS_SYMLINK
	rc = lchown(fsm->path, st->st_uid, st->st_gid);
if (rc < 0) fprintf(stderr, "*** %s(%s,%d,%d) %m\n", cur, fsm->path, st->st_uid, st->st_gid);
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
#endif
	break;
    case FSM_CHMOD:
	rc = chmod(fsm->path, (st->st_mode & 07777));
if (rc < 0) fprintf(stderr, "*** %s(%s,%o) %m\n", cur, fsm->path, (st->st_mode & 07777));
	if (rc < 0)	rc = CPIOERR_CHMOD_FAILED;
	break;
    case FSM_UTIME:
	{   struct utimbuf stamp;
	    stamp.actime = st->st_mtime;
	    stamp.modtime = st->st_mtime;
	    rc = utime(fsm->path, &stamp);
if (rc < 0) fprintf(stderr, "*** %s(%s,%x) %m\n", cur, fsm->path, (unsigned)st->st_mtime);
	    if (rc < 0)	rc = CPIOERR_UTIME_FAILED;
	}
	break;
    case FSM_SYMLINK:
	rc = symlink(fsm->opath, fsm->path);
if (rc < 0) fprintf(stderr, "*** %s(%s,%s) %m\n", cur, fsm->opath, fsm->path);
	if (rc < 0)	rc = CPIOERR_SYMLINK_FAILED;
	break;
    case FSM_LINK:
	rc = link(fsm->opath, fsm->path);
if (rc < 0) fprintf(stderr, "*** %s(%s,%s) %m\n", cur, fsm->opath, fsm->path);
	if (rc < 0)	rc = CPIOERR_LINK_FAILED;
	break;
    case FSM_MKFIFO:
	rc = mkfifo(fsm->path, (st->st_mode & 07777));
if (rc < 0) fprintf(stderr, "*** %s(%s,%o) %m\n", cur, fsm->path, (st->st_mode & 07777));
	if (rc < 0)	rc = CPIOERR_MKFIFO_FAILED;
	break;
    case FSM_MKNOD:
	/*@-unrecog@*/
	rc = mknod(fsm->path, (st->st_mode & ~07777), st->st_rdev);
if (rc < 0) fprintf(stderr, "*** %s(%s,%o,%x) %m\n", cur, fsm->path, (st->st_mode & ~07777), (unsigned)st->st_rdev);
	if (rc < 0)	rc = CPIOERR_MKNOD_FAILED;
	/*@=unrecog@*/
	break;
    case FSM_LSTAT:
	rc = lstat(fsm->path, ost);
if (rc < 0 && errno != ENOENT) fprintf(stderr, "*** %s(%s,%p) %m\n", cur, fsm->path, ost);
	if (rc < 0)	rc = CPIOERR_LSTAT_FAILED;
	break;
    case FSM_STAT:
	rc = stat(fsm->path, ost);
if (rc < 0 && errno != ENOENT) fprintf(stderr, "*** %s(%s,%p) %m\n", cur, fsm->path, ost);
	if (rc < 0)	rc = CPIOERR_STAT_FAILED;
	break;
    case FSM_CHROOT:
	break;

    case FSM_NEXT:
	rc = getNextHeader(fsm, st);	/* Read next payload header. */
	if (rc) break;
	if (!strcmp(fsm->path, TRAILER)) { /* Detect end-of-payload. */
	    fsm->path = _free(fsm->path);
	    rc = CPIOERR_HDR_TRAILER;
	}
	break;
    case FSM_EAT:
	{   char buf[BUFSIZ];		/* XXX add to fsm */
	    int amount;
	    int bite;

	    rc = 0;
	    for (amount = st->st_size; amount > 0; amount -= bite) {
		bite = (amount > sizeof(buf)) ? sizeof(buf) : amount;
		if (ourread(fsm->cfd, buf, bite) == bite)
		    continue;
		rc = CPIOERR_READ_FAILED;
		break;
	    }
	}
	break;
    case FSM_POS:
	{   int buf[16];		/* XXX add to fsm */
	    int modulo = 4;
	    int amount;

	    amount = (modulo - fdGetCpioPos(fsm->cfd) % modulo) % modulo;
	    if (amount)
		(void) ourread(fsm->cfd, buf, amount);
	}
	break;
    case FSM_PAD:
	break;

    default:
	break;
    }

    if (!(stage & FSM_INTERNAL)) {
	fsm->rc = (rc == CPIOERR_HDR_TRAILER ? 0 : rc);
    }
    return rc;
}

int fsmSetup(FSM_t fsm, const rpmTransactionSet ts, const TFI_t fi, FD_t cfd,
		unsigned int * archiveSize, const char ** failedFile)
{
    int rc = fsmStage(fsm, FSM_CREATE);

    if (cfd) {
	fsm->cfd = fdLink(cfd, "persist (fsm)");
	fdSetCpioPos(fsm->cfd, 0);
    }
    fsm->mapi = mapInitIterator(ts, fi);
    fsm->archiveSize = archiveSize;
    fsm->failedFile = failedFile;
    if (fsm->failedFile)
	*fsm->failedFile = NULL;
    if (ts->id > 0)
	sprintf(fsm->sufbuf, ";%08x", ts->id);
    return rc;
}

int fsmTeardown(FSM_t fsm) {
    int rc = 0;
    fsm->mapi = mapFreeIterator(fsm->mapi);
    if (fsm->cfd) {
	fsm->cfd = fdFree(fsm->cfd, "persist (fsm)");
	fsm->cfd = NULL;
    }
    fsm->failedFile = NULL;
    return rc;
}

rpmTransactionSet fsmGetTs(FSM_t fsm) {
    struct mapi * mapi = fsm->mapi;
    return (mapi ? mapi->ts : NULL);
}

TFI_t fsmGetFi(FSM_t fsm) {
    struct mapi * mapi = fsm->mapi;
    return (mapi ? mapi->fi : NULL);
}

int fsmGetIndex(FSM_t fsm) {
    struct mapi * mapi = fsm->mapi;
    return (mapi ? mapi->isave : -1);
}

fileAction fsmAction(FSM_t fsm,
	/*@out@*/ const char ** osuffix, /*@out@*/ const char ** nsuffix)
{
    TFI_t fi = fsmGetFi(fsm);
    int i = fsmGetIndex(fsm);
    fileAction action = FA_UNKNOWN;

    if (fsm->map && fi && i >= 0 && i < fi->fc) {
	action = fi->actions[i];
	if (osuffix) *osuffix = NULL;
	if (nsuffix) *nsuffix = NULL;
	switch (action) {
	case FA_SKIP:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    break;
	case FA_SKIPMULTILIB:	/* XXX RPMFILE_STATE_MULTILIB? */
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    break;
	case FA_UNKNOWN:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    break;

	case FA_CREATE:
	    assert(fi->type == TR_ADDED);
	    break;

	case FA_SKIPNSTATE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NOTINSTALLED;
	    break;

	case FA_SKIPNETSHARED:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NETSHARED;
	    break;

	case FA_BACKUP:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    switch (fi->type) {
	    case TR_ADDED:
		if (osuffix) *osuffix = SUFFIX_RPMORIG;
		break;
	    case TR_REMOVED:
		if (osuffix) *osuffix = SUFFIX_RPMSAVE;
		break;
	    }
	    break;

	case FA_ALTNAME:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    assert(fi->type == TR_ADDED);
	    if (nsuffix) *nsuffix = SUFFIX_RPMNEW;
	    break;

	case FA_SAVE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    assert(fi->type == TR_ADDED);
	    if (osuffix) *osuffix = SUFFIX_RPMSAVE;
	    break;
	case FA_REMOVE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    assert(fi->type == TR_REMOVED);
	    break;
	default:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(action), fsm->path);
	    break;
	}
    }
    return action;
}

/** @todo Verify payload MD5 sum. */
int cpioInstallArchive(FSM_t fsm)
{
    int rc = 0;

#ifdef	NOTYET
    char * md5sum = NULL;

    fdInitMD5(cfd, 0);
#endif

    do {

	/* Clean fsm, free'ing memory. Read next archive header. */
	rc = fsmStage(fsm, FSM_INIT);

	/* Exit on end-of-payload. */
	if (rc == CPIOERR_HDR_TRAILER) {
	    rc = 0;
	    break;
	}

	/* Abort on error. */
	if (rc) goto exit;

	/* Create any directories in path. */
	rc = fsmStage(fsm, FSM_PRE);

	/* Extract file from archive. */
	if (!rc)	rc = fsmStage(fsm, FSM_PROCESS);

	/* If successfully extracted, set final file info. */
	if (!rc)	rc = fsmStage(fsm, FSM_POST);

	/* Notify on success. */
	if (!rc)	(void) fsmStage(fsm, FSM_NOTIFY);

	(void) fsmStage(fsm, (rc ? FSM_UNDO : FSM_COMMIT));

    } while (rc == 0);

    rc = fsmStage(fsm, FSM_DESTROY);

#ifdef	NOTYET
    fdFiniMD5(fsm->cfd, (void **)&md5sum, NULL, 1);

    if (md5sum)
	free(md5sum);
#endif

exit:
    return rc;
}

/**
 * Write next item to payload stream.
 * @param st		stat info for item
 * @param map		mapping name and flags for item
 * @retval sizep	address of no. bytes written
 * @param writeData	should data be written?
 * @return		0 on success
 */
static int writeFile(FSM_t fsm, const struct stat * st, /*@out@*/ size_t * sizep,
	int writeData)
	/*@modifies fsm->cfd, *sizep @*/
{
    const rpmTransactionSet ts = fsmGetTs(fsm);
    TFI_t fi = fsmGetFi(fsm);

    const char * fsPath = mapFsPath(fsm->map, NULL, NULL, NULL);
    const char * archivePath = NULL;
    const char * fsmPath = !fsmFlags(fsm, CPIO_MAP_PATH)
		? fsPath : (archivePath = mapArchivePath(fsm->map));
    struct cpioCrcPhysicalHeader hdr;
    char buf[8192], symbuf[2048];
    dev_t num;
    FD_t datafd;
    size_t st_size = st->st_size;	/* XXX hard links need size preserved */
    mode_t st_mode = st->st_mode;
    uid_t st_uid = st->st_uid;
    gid_t st_gid = st->st_gid;
    size_t amount = 0;
    size_t size;
    int rc;

    if (fsmFlags(fsm, CPIO_MAP_MODE))
	st_mode = (st_mode & S_IFMT) | mapFinalMode(fsm->map);
    if (fsmFlags(fsm, CPIO_MAP_UID))
	st_uid = mapFinalUid(fsm->map);
    if (fsmFlags(fsm, CPIO_MAP_GID))
	st_gid = mapFinalGid(fsm->map);

    if (!writeData || S_ISDIR(st_mode)) {
	st_size = 0;
    } else if (S_ISLNK(st_mode)) {
	/*
	 * While linux puts the size of a symlink in the st_size field,
	 * I don't think that's a specified standard.
	 */
	amount = Readlink(fsPath, symbuf, sizeof(symbuf));
	if (amount <= 0) {
	    rc = CPIOERR_READLINK_FAILED;
	    goto exit;
	}

	st_size = amount;
    }

    memcpy(hdr.magic, CPIO_NEWC_MAGIC, sizeof(hdr.magic));
    SET_NUM_FIELD(hdr.inode, st->st_ino, buf);
    SET_NUM_FIELD(hdr.mode, st_mode, buf);
    SET_NUM_FIELD(hdr.uid, st_uid, buf);
    SET_NUM_FIELD(hdr.gid, st_gid, buf);
    SET_NUM_FIELD(hdr.nlink, st->st_nlink, buf);
    SET_NUM_FIELD(hdr.mtime, st->st_mtime, buf);
    SET_NUM_FIELD(hdr.filesize, st_size, buf);

    num = major((unsigned)st->st_dev); SET_NUM_FIELD(hdr.devMajor, num, buf);
    num = minor((unsigned)st->st_dev); SET_NUM_FIELD(hdr.devMinor, num, buf);
    num = major((unsigned)st->st_rdev); SET_NUM_FIELD(hdr.rdevMajor, num, buf);
    num = minor((unsigned)st->st_rdev); SET_NUM_FIELD(hdr.rdevMinor, num, buf);

    num = strlen(fsmPath) + 1; SET_NUM_FIELD(hdr.namesize, num, buf);
    memcpy(hdr.checksum, "00000000", 8);

    if ((rc = safewrite(fsm->cfd, &hdr, PHYS_HDR_SIZE)) != PHYS_HDR_SIZE)
	goto exit;
    if ((rc = safewrite(fsm->cfd, fsmPath, num)) != num)
	goto exit;
    size = PHYS_HDR_SIZE + num;
    if ((rc = padoutfd(fsm->cfd, &size, 4)))
	goto exit;

    if (writeData && S_ISREG(st_mode)) {
	char *b;
#if HAVE_MMAP
	void *mapped;
	size_t nmapped;
#endif

	/* XXX unbuffered mmap generates *lots* of fdio debugging */
	datafd = Fopen(fsPath, "r.ufdio");
	if (datafd == NULL || Ferror(datafd)) {
	    rc = CPIOERR_OPEN_FAILED;
	    goto exit;
	}

#if HAVE_MMAP
	nmapped = 0;
	mapped = mmap(NULL, st_size, PROT_READ, MAP_SHARED, Fileno(datafd), 0);
	if (mapped != (void *)-1) {
	    b = (char *)mapped;
	    nmapped = st_size;
	} else
#endif
	{
	    b = buf;
	}

	size += st_size;

	while (st_size) {
#if HAVE_MMAP
	  if (mapped != (void *)-1) {
	    amount = nmapped;
	  } else
#endif
	  {
	    amount = Fread(b, sizeof(buf[0]),
			(st_size > sizeof(buf) ? sizeof(buf) : st_size),
			datafd);
	    if (amount <= 0) {
		int olderrno = errno;
		Fclose(datafd);
		errno = olderrno;
		rc = CPIOERR_READ_FAILED;
		goto exit;
	    }
	  }

	    if ((rc = safewrite(fsm->cfd, b, amount)) != amount) {
		int olderrno = errno;
		Fclose(datafd);
		errno = olderrno;
		goto exit;
	    }

	    st_size -= amount;
	}

#if HAVE_MMAP
	if (mapped != (void *)-1) {
	    /*@-noeffect@*/ munmap(mapped, nmapped) /*@=noeffect@*/;
	}
#endif

	Fclose(datafd);
    } else if (writeData && S_ISLNK(st_mode)) {
	if ((rc = safewrite(fsm->cfd, symbuf, amount)) != amount)
	    goto exit;
	size += amount;
    }

    /* this is a noop for most file types */
    if ((rc = padoutfd(fsm->cfd, &size, 4)))
	goto exit;

    if (sizep)
	*sizep = size;

    if (ts && fi && ts->notify) {
	(void)ts->notify(fi->h, RPMCALLBACK_INST_PROGRESS, size, size,
			(fi->ap ? fi->ap->key : NULL), ts->notifyData);
    }

    rc = 0;

exit:
    fsPath = _free(fsPath);
    return rc;
}

/**
 * Write set of linked files to payload stream.
 * @param hlink		set of linked files
 * @retval sizep	address of no. bytes written
 * @return		0 on success
 */
static int writeLinkedFile(FSM_t fsm,
		const struct hardLink * hlink, /*@out@*/size_t * sizep)
	/*@modifies fsm->cfd, *sizep, *fsm->failedFile @*/
{
    size_t total = 0;
    size_t size;
    int rc = 0;
    int i;

    for (i = hlink->nlink - 1; i > hlink->linksLeft; i--) {
	fsm->map = hlink->fileMaps[i];
	if ((rc = writeFile(fsm, &hlink->sb, &size, 0)) != 0)
	    goto exit;
	total += size;
	fsm->map = mapFree(fsm->map);
    }

    i = hlink->linksLeft;
    fsm->map = hlink->fileMaps[i];
    if ((rc = writeFile(fsm, &hlink->sb, &size, 1))) {
	if (sizep)
	    *sizep = total;
	goto exit;
    }
    total += size;

    if (sizep)
	*sizep = total;

    rc = 0;

exit:
    if (rc && fsm->failedFile && *fsm->failedFile == NULL)
	*fsm->failedFile = mapFsPath(fsm->map, NULL, NULL, NULL);
    fsm->map = mapFree(fsm->map);
    return rc;
}

int cpioBuildArchive(FSM_t fsm)
{
/*@-fullinitblock@*/
    struct hardLink hlinkList = { NULL };
/*@=fullinitblock@*/
    struct stat * st = (struct stat *) &hlinkList.sb;
    struct hardLink * hlink;
    size_t archiveSize = 0;
    size_t size;
    int rc;

    hlinkList.next = NULL;

    while ((fsm->map = mapNextIterator(fsm->mapi)) != NULL) {

	fsm->path = mapFsPath(fsm->map, NULL, NULL, NULL);

	if (fsmFlags(fsm, CPIO_FOLLOW_SYMLINKS))
	    rc = Stat(fsm->path, st);
	else
	    rc = Lstat(fsm->path, st);

	if (rc) {
	    
	    rc = CPIOERR_STAT_FAILED;
	    goto exit;
	}

	if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
	    hlink = hlinkList.next;
	    while (hlink &&
		  (hlink->dev != st->st_dev || hlink->inode != st->st_ino))
			hlink = hlink->next;
	    if (hlink == NULL) {
		hlink = newHardLink(st, HARDLINK_BUILD);
		hlink->next = hlinkList.next;
		hlinkList.next = hlink;
	    }

	    hlink->fileMaps[--hlink->linksLeft] = mapLink(fsm->map);

	    if (hlink->linksLeft == 0) {
		struct hardLink * prev;
		rc = writeLinkedFile(fsm, hlink, &size);
		if (rc) {
		    fsm->path = _free(fsm->path);
		    goto exit;
		}

		archiveSize += size;

		prev = &hlinkList;
		do {
		    if (prev->next != hlink)
			continue;
		    prev->next = hlink->next;
		    hlink->next = NULL;
		    hlink = freeHardLink(hlink);
		    break;
		} while ((prev = prev->next) != NULL);
	    }
	} else {
	    if ((rc = writeFile(fsm, st, &size, 1)))
		goto exit;
	    archiveSize += size;
	}
	fsm->path = _free(fsm->path);
    }

    rc = 0;
    while ((hlink = hlinkList.next) != NULL) {
	hlinkList.next = hlink->next;
	hlink->next = NULL;
	if (rc == 0) {
	    rc = writeLinkedFile(fsm, hlink, &size);
	    archiveSize += size;
	}
	hlink = freeHardLink(hlink);
    }
    if (rc)
	goto exit;

    {	struct cpioCrcPhysicalHeader hdr;
	memset(&hdr, '0', PHYS_HDR_SIZE);
	memcpy(hdr.magic, CPIO_NEWC_MAGIC, sizeof(hdr.magic));
	memcpy(hdr.nlink, "00000001", 8);
	memcpy(hdr.namesize, "0000000b", 8);
	if ((rc = safewrite(fsm->cfd, &hdr, PHYS_HDR_SIZE)) != PHYS_HDR_SIZE)
	    goto exit;
	if ((rc = safewrite(fsm->cfd, "TRAILER!!!", 11)) != 11)
	    goto exit;
	archiveSize += PHYS_HDR_SIZE + 11;
    }

    /* GNU cpio pads to 512 bytes here, but we don't. I'm not sure if
       it matters or not */

    rc = padoutfd(fsm->cfd, &archiveSize, 4);

    if (!rc && fsm->archiveSize) *fsm->archiveSize = archiveSize;

exit:
    if (rc && fsm->path && fsm->failedFile && *fsm->failedFile == NULL)
	*fsm->failedFile = xstrdup(fsm->path);
    fsm->path = _free(fsm->path);
    return rc;
}

const char *const cpioStrerror(int rc)
{
    static char msg[256];
    char *s;
    int l, myerrno = errno;

    strcpy(msg, "cpio: ");
    switch (rc) {
    default:
	s = msg + strlen(msg);
	sprintf(s, _("(error 0x%x)"), (unsigned)rc);
	s = NULL;
	break;
    case CPIOERR_BAD_MAGIC:	s = _("Bad magic");		break;
    case CPIOERR_BAD_HEADER:	s = _("Bad/unreadable  header");break;

    case CPIOERR_OPEN_FAILED:	s = "open";	break;
    case CPIOERR_CHMOD_FAILED:	s = "chmod";	break;
    case CPIOERR_CHOWN_FAILED:	s = "chown";	break;
    case CPIOERR_WRITE_FAILED:	s = "write";	break;
    case CPIOERR_UTIME_FAILED:	s = "utime";	break;
    case CPIOERR_UNLINK_FAILED:	s = "unlink";	break;
    case CPIOERR_RENAME_FAILED:	s = "rename";	break;
    case CPIOERR_SYMLINK_FAILED: s = "symlink";	break;
    case CPIOERR_STAT_FAILED:	s = "stat";	break;
    case CPIOERR_LSTAT_FAILED:	s = "lstat";	break;
    case CPIOERR_MKDIR_FAILED:	s = "mkdir";	break;
    case CPIOERR_RMDIR_FAILED:	s = "rmdir";	break;
    case CPIOERR_MKNOD_FAILED:	s = "mknod";	break;
    case CPIOERR_MKFIFO_FAILED:	s = "mkfifo";	break;
    case CPIOERR_LINK_FAILED:	s = "link";	break;
    case CPIOERR_READLINK_FAILED: s = "readlink";	break;
    case CPIOERR_READ_FAILED:	s = "read";	break;
    case CPIOERR_COPY_FAILED:	s = "copy";	break;

    case CPIOERR_HDR_SIZE:	s = _("Header size too big");	break;
    case CPIOERR_UNKNOWN_FILETYPE: s = _("Unknown file type");	break;
    case CPIOERR_MISSING_HARDLINK: s = _("Missing hard link");	break;
    case CPIOERR_MD5SUM_MISMATCH: s = _("MD5 sum mismatch");	break;
    case CPIOERR_INTERNAL:	s = _("Internal error");	break;
    }

    l = sizeof(msg) - strlen(msg) - 1;
    if (s != NULL) {
	if (l > 0) strncat(msg, s, l);
	l -= strlen(s);
    }
    if ((rc & CPIOERR_CHECK_ERRNO) && myerrno) {
	s = _(" failed - ");
	if (l > 0) strncat(msg, s, l);
	l -= strlen(s);
	if (l > 0) strncat(msg, strerror(myerrno), l);
    }
    return msg;
}
