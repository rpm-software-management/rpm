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

/*@access FD_t @*/
/*@access rpmTransactionSet @*/
/*@access TFI_t @*/
/*@access FSM_t @*/

#define CPIO_NEWC_MAGIC	"070701"
#define CPIO_CRC_MAGIC	"070702"
#define CPIO_TRAILER	"TRAILER!!!"

#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

static /*@null@*/ void * _free(/*@only@*/ /*@null@*/ const void * this) {
    if (this)	free((void *)this);
    return NULL;
}

int _fsm_debug = 0;

/** \ingroup payload
 * Keeps track of the set of all hard links to a file in an archive.
 */
struct hardLink {
/*@owned@*/ struct hardLink * next;
/*@owned@*/ const char ** files;	/* nlink of these, used by install */
/*@owned@*/ const char ** nsuffix;
/*@owned@*/ int * filex;
    dev_t dev;
    ino_t inode;
    int nlink;
    int linksLeft;
    int linkIndex;
    int createdPath;
};

/**
 */
typedef struct fsmIterator_s {
/*@kept@*/ rpmTransactionSet ts;	/*!< transaction set. */
/*@kept@*/ TFI_t fi;			/*!< transaction element file info. */
    int isave;				/*!< last returned iterator index. */
    int i;				/*!< iterator index. */
} * FSMI_t;

/** \ingroup payload
 * File name and stat information.
 */
struct fsm_s {
/*@owned@*/ const char * path;		/*!< Current file name. */
/*@owned@*/ const char * opath;		/*!< Original file name. */
    FD_t cfd;				/*!< Payload file handle. */
    FD_t rfd;				/*!<  read: File handle. */
/*@dependent@*/ char * rdbuf;		/*!<  read: Buffer. */
/*@owned@*/ char * rdb;			/*!<  read: Buffer allocated. */
    size_t rdsize;			/*!<  read: Buffer allocated size. */
    size_t rdlen;			/*!<  read: Number of bytes requested. */
    size_t rdnb;			/*!<  read: Number of bytes returned. */
    FD_t wfd;				/*!< write: File handle. */
/*@dependent@*/ char * wrbuf;		/*!< write: Buffer. */
/*@owned@*/ char * wrb;			/*!< write: Buffer allocated. */
    size_t wrsize;			/*!< write: Buffer allocated size. */
    size_t wrlen;			/*!< write: Number of bytes requested. */
    size_t wrnb;			/*!< write: Number of bytes returned. */
/*@only@*/ FSMI_t iter;			/*!< File iterator. */
    int ix;				/*!< Current file iterator index. */
/*@only@*/ struct hardLink * links;	/*!< Pending hard linked file(s). */
/*@only@*/ struct hardLink * li;	/*!< Current hard linked file(s). */
/*@kept@*/ unsigned int * archiveSize;	/*!< Pointer to archive size. */
/*@kept@*/ const char ** failedFile;	/*!< First file name that failed. */
/*@shared@*/ const char * subdir;	/*!< Current file sub-directory. */
    char subbuf[64];	/* XXX eliminate */
/*@observer@*/ const char * osuffix;	/*!< Old, preserved, file suffix. */
/*@observer@*/ const char * nsuffix;	/*!< New, created, file suffix. */
/*@shared@*/ const char * suffix;	/*!< Current file suffix. */
    char sufbuf[64];	/* XXX eliminate */
/*@only@*/ short * dnlx;		/*!< Last dirpath verified indexes. */
/*@only@*/ char * ldn;			/*!< Last dirpath verified. */
    int ldnlen;				/*!< Last dirpath current length. */
    int ldnalloc;			/*!< Last dirpath allocated length. */
    int postpone;			/*!< Skip remaining stages? */
    int diskchecked;			/*!< Has stat(2) been performed? */
    int exists;				/*!< Does current file exist on disk? */
    int mkdirsdone;			/*!< Have "orphan" dirs been created? */
    int astriplen;			/*!< Length of buildroot prefix. */
    int rc;				/*!< External file stage return code. */
    int commit;				/*!< Commit synchronously? */
    cpioMapFlags mapFlags;		/*!< Bit(s) to control mapping. */
/*@shared@*/ const char * archivePath;	/*!< Path to store in cpio archive. */
/*@shared@*/ const char * dirName;	/*!< File directory name. */
/*@shared@*/ const char * baseName;	/*!< File base name. */
/*@shared@*/ const char * fmd5sum;	/*!< File MD5 sum (NULL disables). */
    fileAction action;			/*!< File disposition. */
    fileStage goal;			/*!< Install/build/erase */
    fileStage stage;			/*!< External file stage. */
    struct stat sb;			/*!< Current file stat(2) info. */
    struct stat osb;			/*!< Original file stat(2) info. */
};

rpmTransactionSet fsmGetTs(const FSM_t fsm) {
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->ts : NULL);
}

TFI_t fsmGetFi(const FSM_t fsm) {
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->fi : NULL);
}

#ifdef	UNUSED
static int fsmSetIndex(FSM_t fsm, int isave) {
    FSMI_t iter = fsm->iter;
    int iprev = -1;
    if (iter) {
	iprev = iter->isave;
	iter->isave = isave;
    }
    return iprev;
}

int fsmGetIndex(FSM_t fsm) {
    const FSMI_t iter = fsm->iter;
    return (iter ? iter->isave : -1);
}
#endif

#define	SUFFIX_RPMORIG	".rpmorig"
#define	SUFFIX_RPMSAVE	".rpmsave"
#define	SUFFIX_RPMNEW	".rpmnew"

/**
 */
static /*@only@*//*@null@*/ const char * fsmFsPath(/*@null@*/ const FSM_t fsm,
	/*@null@*/ const struct stat * st,
	/*@null@*/ const char * subdir,
	/*@null@*/ const char * suffix)
{
    const char * s = NULL;

    if (fsm) {
	int nb;
	char * t;
	nb = strlen(fsm->dirName) +
	    (st && subdir && !S_ISDIR(st->st_mode) ? strlen(subdir) : 0) +
	    (st && suffix && !S_ISDIR(st->st_mode) ? strlen(suffix) : 0) +
	    strlen(fsm->baseName) + 1;
	s = t = xmalloc(nb);
	t = stpcpy(t, fsm->dirName);
	if (st && subdir && !S_ISDIR(st->st_mode))
	    t = stpcpy(t, subdir);
	t = stpcpy(t, fsm->baseName);
	if (st && suffix && !S_ISDIR(st->st_mode))
	    t = stpcpy(t, suffix);
    }
    return s;
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
    rpmTransactionSet ts = (void *)this;
    TFI_t fi = (void *)that;
    FSMI_t iter = NULL;

    if (fi) {
	iter = xcalloc(1, sizeof(*iter));
	iter->ts = ts;
	iter->fi = fi;
	iter->isave = iter->i = 0;

	if (ts && ts->notify) {
	    (void)ts->notify(fi->h, RPMCALLBACK_INST_START, 0, fi->archiveSize,
		(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
    }

    return iter;
}

/**
 */
static int mapNextIterator(void * this) {
    FSMI_t iter = this;
    const TFI_t fi = iter->fi;
    int i;

    do {
	if (!((i = iter->i) < fi->fc))
	    return -1;
	iter->i++;
    } while (0);

    iter->isave = i;

    return i;
}

int pkgAction(const rpmTransactionSet ts, TFI_t fi, int i, /*@unused@*/fileStage a)
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

    rpmMessage(RPMMESS_DEBUG, _("    file: %s%s action: %s\n"),
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

typedef struct dnli_s {
/*@dependent@*/ TFI_t fi;
/*@only@*/ /*@null@*/ char * active;
    int reverse;
    int isave;
    int i;
} * DNLI_t;

/**
 */
static /*@null@*/ void * dnlFreeIterator(/*@only@*//*@null@*/ const void * this)
{
    if (this) {
	DNLI_t dnli = (void *)this;
	if (dnli->active) free(dnli->active);
    }
    return _free(this);
}

static inline int dnlCount(const DNLI_t dnli) {
    return (dnli ? dnli->fi->dc : 0);
}

static inline int dnlIndex(const DNLI_t dnli) {
    return (dnli ? dnli->isave : -1);
}

/**
 */
static /*@only@*/ void * dnlInitIterator(const FSM_t fsm, int reverse)
{
    TFI_t fi = fsmGetFi(fsm);
    DNLI_t dnli;
    int i, j;

    if (fi == NULL)
	return NULL;
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
		/* This directory is included in the package. */
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
static const char * dnlNextIterator(/*@null@*/ DNLI_t dnli) {
    const char * dn = NULL;

    if (dnli && dnli->active) {
	TFI_t fi = dnli->fi;
	int i = -1;

	do {
	    i = (!dnli->reverse ? dnli->i++ : --dnli->i);
	} while (i >= 0 && i < fi->dc && !dnli->active[i]);

	if (i >= 0 && i < fi->dc)
	    dn = fi->dnl[i];
	else
	    i = -1;
	dnli->isave = i;
    }
    return dn;
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
static int mapFind(void * this, const char * fsmPath) {
    FSMI_t iter = this;
    const TFI_t fi = iter->fi;
    int ix = -1;

    if (fi) {
	const char ** p;

	p = bsearch(&fsmPath, fi->apath, fi->fc, sizeof(fsmPath), cpioStrCmp);
	if (p == NULL) {
	    fprintf(stderr, "*** not mapped %s\n", fsmPath);
	} else {
	    iter->i = p - fi->apath;
	    ix = mapNextIterator(iter);
	}
    }
    return ix;
}

/**
 * Save hard link in chain.
 * @return		Is chain only partially filled?
 */
static int saveHardLink(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    int j;

    /* Find hard link set. */
    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->inode == st->st_ino && fsm->li->dev == st->st_dev)
	    break;
    }

    /* New har link encountered, add new set. */
    if (fsm->li == NULL) {
	fsm->li = xcalloc(1, sizeof(*fsm->li));
	fsm->li->next = NULL;
	fsm->li->nlink = st->st_nlink;
	fsm->li->dev = st->st_dev;
	fsm->li->inode = st->st_ino;
	fsm->li->linkIndex = -1;
	fsm->li->createdPath = -1;

	fsm->li->filex = xcalloc(st->st_nlink, sizeof(fsm->li->filex[0]));
	memset(fsm->li->filex, -1, (st->st_nlink * sizeof(fsm->li->filex[0])));
	fsm->li->nsuffix = xcalloc(st->st_nlink, sizeof(*fsm->li->nsuffix));
	fsm->li->files = xcalloc(st->st_nlink, sizeof(*fsm->li->files));

	if (fsm->goal == FSM_BUILD)
	    fsm->li->linksLeft = st->st_nlink;
	if (fsm->goal == FSM_INSTALL)
	    fsm->li->linksLeft = 0;

	fsm->li->next = fsm->links;
	fsm->links = fsm->li;
    }

    if (fsm->goal == FSM_BUILD) --fsm->li->linksLeft;
    fsm->li->filex[fsm->li->linksLeft] = fsm->ix;
    fsm->li->files[fsm->li->linksLeft] = xstrdup(fsm->path);
    /*@-observertrans@*/
    fsm->li->nsuffix[fsm->li->linksLeft] = fsm->nsuffix;
    /*@=observertrans@*/
    if (fsm->goal == FSM_INSTALL) fsm->li->linksLeft++;

#if 0
fprintf(stderr, "*** %p link[%d:%d] %d filex %d %s\n", fsm->li, fsm->li->linksLeft, st->st_nlink, (int)st->st_size, fsm->li->filex[fsm->li->linksLeft], fsm->li->files[fsm->li->linksLeft]);
#endif

    if (fsm->goal == FSM_BUILD)
	return (fsm->li->linksLeft > 0);

    if (fsm->goal != FSM_INSTALL)
	return 0;

    if (!(st->st_size || fsm->li->linksLeft == st->st_nlink))
	return 1;

    /* Here come the bits, time to choose a non-skipped file name. */
    {	TFI_t fi = fsmGetFi(fsm);

	for (j = fsm->li->linksLeft - 1; j >= 0; j--) {
	    int i;
	    i = fsm->li->filex[j];
	    if (i < 0 || XFA_SKIPPING(fi->actions[i]))
		continue;
	    break;
	}
    }

    /* Are all links skipped or not encountered yet? */
    if (j < 0)
	return 1;	/* XXX W2DO? */

    /* Save the non-skipped file name and map index. */
    fsm->li->linkIndex = j;
    fsm->path = _free(fsm->path);
    fsm->path = xstrdup(fsm->li->files[j]);
    return 0;
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
	fsm->iter = mapFreeIterator(fsm->iter);
    }
    return _free(fsm);
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

#define GET_NUM_FIELD(phys, log) \
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	if (*end) return CPIOERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	memcpy(phys, space, 8);

static int cpioTrailerWrite(FSM_t fsm)
{
    struct cpioCrcPhysicalHeader * hdr =
	(struct cpioCrcPhysicalHeader *)fsm->rdbuf;
    int rc;

    memset(hdr, '0', PHYS_HDR_SIZE);
    memcpy(hdr->magic, CPIO_NEWC_MAGIC, sizeof(hdr->magic));
    memcpy(hdr->nlink, "00000001", 8);
    memcpy(hdr->namesize, "0000000b", 8);
    memcpy(fsm->rdbuf + PHYS_HDR_SIZE, CPIO_TRAILER, sizeof(CPIO_TRAILER));

    /* XXX DWRITE uses rdnb for I/O length. */
    fsm->rdnb = PHYS_HDR_SIZE + sizeof(CPIO_TRAILER);
    rc = fsmStage(fsm, FSM_DWRITE);

    /*
     * GNU cpio pads to 512 bytes here, but we don't. This may matter for
     * tape device(s) and/or concatenated cpio archives. <shrug>
     */
    if (!rc)
	rc = fsmStage(fsm, FSM_PAD);

    return rc;
}

/**
 * Write cpio header.
 * @retval fsm		file path and stat info
 * @return		0 on success
 */
static int cpioHeaderWrite(FSM_t fsm, struct stat * st)
{
    struct cpioCrcPhysicalHeader * hdr = (struct cpioCrcPhysicalHeader *)fsm->rdbuf;
    char field[64];
    size_t len;
    dev_t dev;
    int rc = 0;

    memcpy(hdr->magic, CPIO_NEWC_MAGIC, sizeof(hdr->magic));
    SET_NUM_FIELD(hdr->inode, st->st_ino, field);
    SET_NUM_FIELD(hdr->mode, st->st_mode, field);
    SET_NUM_FIELD(hdr->uid, st->st_uid, field);
    SET_NUM_FIELD(hdr->gid, st->st_gid, field);
    SET_NUM_FIELD(hdr->nlink, st->st_nlink, field);
    SET_NUM_FIELD(hdr->mtime, st->st_mtime, field);
    SET_NUM_FIELD(hdr->filesize, st->st_size, field);

    dev = major((unsigned)st->st_dev); SET_NUM_FIELD(hdr->devMajor, dev, field);
    dev = minor((unsigned)st->st_dev); SET_NUM_FIELD(hdr->devMinor, dev, field);
    dev = major((unsigned)st->st_rdev); SET_NUM_FIELD(hdr->rdevMajor, dev, field);
    dev = minor((unsigned)st->st_rdev); SET_NUM_FIELD(hdr->rdevMinor, dev, field);

    len = strlen(fsm->path) + 1; SET_NUM_FIELD(hdr->namesize, len, field);
    memcpy(hdr->checksum, "00000000", 8);
    memcpy(fsm->rdbuf + PHYS_HDR_SIZE, fsm->path, len);

    /* XXX DWRITE uses rdnb for I/O length. */
    fsm->rdnb = PHYS_HDR_SIZE + len;
    rc = fsmStage(fsm, FSM_DWRITE);
    if (!rc && fsm->rdnb != fsm->wrnb)
	rc = CPIOERR_WRITE_FAILED;
    if (!rc)
	rc = fsmStage(fsm, FSM_PAD);
    return rc;
}

/**
 * Read cpio heasder.
 * @retval fsm		file path and stat info
 * @return		0 on success
 */
static int cpioHeaderRead(FSM_t fsm, struct stat * st)
	/*@modifies fsm, *st @*/
{
    struct cpioCrcPhysicalHeader hdr;
    int nameSize;
    char * end;
    int major, minor;
    int rc = 0;

    fsm->wrlen = PHYS_HDR_SIZE;
    rc = fsmStage(fsm, FSM_DREAD);
    if (!rc && fsm->rdnb != fsm->wrlen)
	rc = CPIOERR_READ_FAILED;
    if (rc) return rc;
    memcpy(&hdr, fsm->wrbuf, fsm->rdnb);

    if (strncmp(CPIO_CRC_MAGIC, hdr.magic, sizeof(CPIO_CRC_MAGIC)-1) &&
	strncmp(CPIO_NEWC_MAGIC, hdr.magic, sizeof(CPIO_NEWC_MAGIC)-1))
	return CPIOERR_BAD_MAGIC;

    GET_NUM_FIELD(hdr.inode, st->st_ino);
    GET_NUM_FIELD(hdr.mode, st->st_mode);
    GET_NUM_FIELD(hdr.uid, st->st_uid);
    GET_NUM_FIELD(hdr.gid, st->st_gid);
    GET_NUM_FIELD(hdr.nlink, st->st_nlink);
    GET_NUM_FIELD(hdr.mtime, st->st_mtime);
    GET_NUM_FIELD(hdr.filesize, st->st_size);

    GET_NUM_FIELD(hdr.devMajor, major);
    GET_NUM_FIELD(hdr.devMinor, minor);
    st->st_dev = /*@-shiftsigned@*/ makedev(major, minor) /*@=shiftsigned@*/ ;

    GET_NUM_FIELD(hdr.rdevMajor, major);
    GET_NUM_FIELD(hdr.rdevMinor, minor);
    st->st_rdev = /*@-shiftsigned@*/ makedev(major, minor) /*@=shiftsigned@*/ ;

    GET_NUM_FIELD(hdr.namesize, nameSize);
    if (nameSize >= fsm->wrsize)
	return CPIOERR_BAD_HEADER;

    {	char * t = xmalloc(nameSize + 1);
	fsm->wrlen = nameSize;
	rc = fsmStage(fsm, FSM_DREAD);
	if (!rc && fsm->rdnb != fsm->wrlen)
	    rc = CPIOERR_BAD_HEADER;
	if (rc) {
	    free(t);
	    fsm->path = NULL;
	    return rc;
	}
	memcpy(t, fsm->wrbuf, fsm->rdnb);
	t[nameSize] = '\0';
	fsm->path = t;
    }

    return 0;
}

int fsmSetup(FSM_t fsm, fileStage goal,
		const rpmTransactionSet ts, const TFI_t fi, FD_t cfd,
		unsigned int * archiveSize, const char ** failedFile)
{
    int rc = fsmStage(fsm, FSM_CREATE);

#if 0
    rpmSetVerbosity(RPMMESS_DEBUG);
#endif

    fsm->goal = goal;
    if (cfd) {
	fsm->cfd = fdLink(cfd, "persist (fsm)");
	fdSetCpioPos(fsm->cfd, 0);
    }
    fsm->iter = mapInitIterator(ts, fi);
    fsm->archiveSize = archiveSize;
    if (fsm->archiveSize)
	*fsm->archiveSize = 0;
    fsm->failedFile = failedFile;
    if (fsm->failedFile)
	*fsm->failedFile = NULL;

    memset(fsm->sufbuf, 0, sizeof(fsm->sufbuf));
    if (fsm->goal != FSM_BUILD) {
	if (ts->id > 0)
	    sprintf(fsm->sufbuf, ";%08x", (unsigned)ts->id);
    }
    return rc;
}

int fsmTeardown(FSM_t fsm) {
    int rc = 0;
    fsm->iter = mapFreeIterator(fsm->iter);
    if (fsm->cfd) {
	fsm->cfd = fdFree(fsm->cfd, "persist (fsm)");
	fsm->cfd = NULL;
    }
    fsm->failedFile = NULL;
    return rc;
}

int fsmMapPath(FSM_t fsm)
{
    const rpmTransactionSet ts = fsmGetTs(fsm);
    TFI_t fi = fsmGetFi(fsm);	/* XXX const except for fstates */
    int rc = 0;
    int i;

    fsm->osuffix = NULL;
    fsm->nsuffix = NULL;
    fsm->astriplen = 0;
    fsm->action = FA_UNKNOWN;
#define	_tsmask	(RPMTRANS_FLAG_PKGCOMMIT | RPMTRANS_FLAG_COMMIT)
    fsm->commit = (ts && (ts->transFlags & _tsmask) ? 0 : 1);
#undef _tsmask
    fsm->mapFlags = 0;

    i = fsm->ix;
    if (fi && i >= 0 && i < fi->fc) {

	fsm->astriplen = fi->astriplen;
	fsm->action = (fi->actions ? fi->actions[i] : FA_UNKNOWN);
	fsm->mapFlags = (fi->fmapflags ? fi->fmapflags[i] : fi->mapflags);

	/* src rpms have simple base name in payload. */
	fsm->archivePath =
		(fi->apath ? fi->apath[i] + fi->striplen : fi->bnl[i]);
	fsm->dirName = fi->dnl[fi->dil[i]];
	fsm->baseName = fi->bnl[i];

	switch (fsm->action) {
	case FA_SKIP:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    break;
	case FA_SKIPMULTILIB:	/* XXX RPMFILE_STATE_MULTILIB? */
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    break;
	case FA_UNKNOWN:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    break;

	case FA_CREATE:
	    assert(fi->type == TR_ADDED);
	    break;

	case FA_SKIPNSTATE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NOTINSTALLED;
	    break;

	case FA_SKIPNETSHARED:
if (_fsm_debug)
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    if (fi->type == TR_ADDED)
		fi->fstates[i] = RPMFILE_STATE_NETSHARED;
	    break;

	case FA_BACKUP:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    switch (fi->type) {
	    case TR_ADDED:
		fsm->osuffix = SUFFIX_RPMORIG;
		break;
	    case TR_REMOVED:
		fsm->osuffix = SUFFIX_RPMSAVE;
		break;
	    }
	    break;

	case FA_ALTNAME:
if (_fsm_debug)
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    assert(fi->type == TR_ADDED);
	    fsm->nsuffix = SUFFIX_RPMNEW;
	    break;

	case FA_SAVE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    assert(fi->type == TR_ADDED);
	    fsm->osuffix = SUFFIX_RPMSAVE;
	    break;
	case FA_REMOVE:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    assert(fi->type == TR_REMOVED);
	    break;
	default:
fprintf(stderr, "*** %s:%s %s\n", fiTypeString(fi), fileActionString(fsm->action), fsm->path);
	    break;
	}

	if ((fsm->mapFlags & CPIO_MAP_PATH) || fsm->nsuffix) {
	    const struct stat * st = &fsm->sb;
	    fsm->path = _free(fsm->path);
	    fsm->path = fsmFsPath(fsm, st, fsm->subdir,
		(fsm->suffix ? fsm->suffix : fsm->nsuffix));
	}
    }
    return rc;
}

int fsmMapAttrs(FSM_t fsm)
{
    struct stat * st = &fsm->sb;
    TFI_t fi = fsmGetFi(fsm);
    int i = fsm->ix;

    if (fi && i >= 0 && i < fi->fc) {
	mode_t perms =
		(S_ISDIR(st->st_mode) ? fi->dperms : fi->fperms);
	mode_t finalMode =
		(fi->fmodes ? fi->fmodes[i] : perms);
	uid_t finalUid =
		(fi->fuids ? fi->fuids[i] : fi->uid); /* XXX chmod u-s */
	gid_t finalGid =
		(fi->fgids ? fi->fgids[i] : fi->gid); /* XXX chmod g-s */

	if (fsm->mapFlags & CPIO_MAP_MODE)
	    st->st_mode = (st->st_mode & S_IFMT) | finalMode;
	if (fsm->mapFlags & CPIO_MAP_UID)
	    st->st_uid = finalUid;
	if (fsm->mapFlags & CPIO_MAP_GID)
	    st->st_gid = finalGid;

	fsm->fmd5sum = (fi->fmd5s ? fi->fmd5s[i] : NULL);

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
		/*@modifies fileSystem, fsm @*/
{
    const char * fmd5sum;
    const struct stat * st = &fsm->sb;
    int left = st->st_size;
    int rc = 0;

    rc = fsmStage(fsm, FSM_WOPEN);
    if (rc)
	goto exit;

    fmd5sum = fsm->fmd5sum;

    /* XXX This doesn't support brokenEndian checks. */
    if (fmd5sum)
	fdInitMD5(fsm->wfd, 0);

    while (left) {

	fsm->wrlen = (left > fsm->wrsize ? fsm->wrsize : left);
	rc = fsmStage(fsm, FSM_DREAD);
	if (rc)
	    goto exit;

	rc = fsmStage(fsm, FSM_WRITE);
	if (rc)
	    goto exit;

	left -= fsm->wrnb;

	/* don't call this with fileSize == fileComplete */
	if (!rc && left)
	    (void) fsmStage(fsm, FSM_NOTIFY);
    }

    if (fmd5sum) {
	const char * md5sum = NULL;

	Fflush(fsm->wfd);
	fdFiniMD5(fsm->wfd, (void **)&md5sum, NULL, 1);

	if (md5sum == NULL) {
	    rc = CPIOERR_MD5SUM_MISMATCH;
	} else {
	    if (strcmp(md5sum, fmd5sum))
		rc = CPIOERR_MD5SUM_MISMATCH;
	    md5sum = _free(md5sum);
	}
    }

exit:
    (void) fsmStage(fsm, FSM_WCLOSE);
    return rc;
}

/**
 * Write next item to payload stream.
 * @retval sizep	address of no. bytes written
 * @param writeData	should data be written?
 * @return		0 on success
 */
static int writeFile(FSM_t fsm, int writeData)
	/*@modifies fsm @*/
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    size_t pos = fdGetCpioPos(fsm->cfd);
    int left;
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
	rc = fsmStage(fsm, FSM_READLINK);
	if (rc) goto exit;
	st->st_size = fsm->rdnb;
    }

    if (fsm->mapFlags & CPIO_MAP_PATH)
	fsm->path = fsm->archivePath;
    rc = fsmStage(fsm, FSM_HWRITE);
    fsm->path = path;
    if (rc) goto exit;

    if (writeData && S_ISREG(st->st_mode)) {
#if HAVE_MMAP
	char * rdbuf = NULL;
	void * mapped = (void *)-1;
	size_t nmapped;
#endif

	rc = fsmStage(fsm, FSM_ROPEN);
	if (rc) goto exit;

	/* XXX unbuffered mmap generates *lots* of fdio debugging */
#if HAVE_MMAP
	nmapped = 0;
	mapped = mmap(NULL, st->st_size, PROT_READ, MAP_SHARED, Fileno(fsm->rfd), 0);
	if (mapped != (void *)-1) {
	    rdbuf = fsm->rdbuf;
	    fsm->rdbuf = (char *) mapped;
	    fsm->rdlen = nmapped = st->st_size;
	}
#endif

	left = st->st_size;

	while (left) {
#if HAVE_MMAP
	  if (mapped != (void *)-1) {
	    fsm->rdnb = nmapped;
	  } else
#endif
	  {
	    fsm->rdlen = (left > fsm->rdsize ? fsm->rdsize : left),
	    rc = fsmStage(fsm, FSM_READ);
	    if (rc) goto exit;
	  }

	    /* XXX DWRITE uses rdnb for I/O length. */
	    rc = fsmStage(fsm, FSM_DWRITE);
	    if (rc) goto exit;

	    left -= fsm->wrnb;
	}

#if HAVE_MMAP
	if (mapped != (void *)-1) {
	    /*@-noeffect@*/ munmap(mapped, nmapped) /*@=noeffect@*/;
	    fsm->rdbuf = rdbuf;
	}
#endif

    } else if (writeData && S_ISLNK(st->st_mode)) {
	/* XXX DWRITE uses rdnb for I/O length. */
	rc = fsmStage(fsm, FSM_DWRITE);
	if (rc) goto exit;
    }

    rc = fsmStage(fsm, FSM_PAD);
    if (rc) goto exit;

    {	const rpmTransactionSet ts = fsmGetTs(fsm);
	TFI_t fi = fsmGetFi(fsm);
	if (ts && fi && ts->notify) {
	    size_t size = (fdGetCpioPos(fsm->cfd) - pos);
	    (void)ts->notify(fi->h, RPMCALLBACK_INST_PROGRESS, size, size,
			(fi->ap ? fi->ap->key : NULL), ts->notifyData);
	}
    }

    rc = 0;

exit:
    if (fsm->rfd)
	(void) fsmStage(fsm, FSM_RCLOSE);
    fsm->opath = opath;
    fsm->path = path;
    return rc;
}

/**
 * Write set of linked files to payload stream.
 * @return		0 on success
 */
static int writeLinkedFile(FSM_t fsm)
	/*@modifies fsm @*/
{
    const char * path = fsm->path;
    int rc = 0;
    int i;

    for (i = fsm->li->nlink - 1; i >= 0; i--) {
	if (fsm->li->filex[i] < 0) continue;
	if (fsm->li->files[i] == NULL) continue;

	fsm->path = fsm->li->files[i];
	fsm->li->filex[i] = -1;
	fsm->li->files[i] = NULL;

	/* Write data after last link. */
	rc = writeFile(fsm, (i == 0));
	if (rc) break;

#ifdef	DYING
	/*@-unqualifiedtrans@*/
	fsm->li->files[i] = _free(fsm->li->files[i]);
	/*@=unqualifiedtrans@*/
#endif
    }

    if (rc && fsm->failedFile && *fsm->failedFile == NULL)
	*fsm->failedFile = xstrdup(fsm->path);

    fsm->path = path;
    return rc;
}

/**
 */
static int fsmMakeLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * opath = fsm->opath;
    int rc = 0;
    int i;

    fsm->opath = fsm->li->files[fsm->li->createdPath];
    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->filex[i] < 0) continue;
	if (fsm->li->files[i] == NULL) continue;
	if (i == fsm->li->createdPath) continue;

	fsm->path = fsm->li->files[i];
	rc = fsmStage(fsm, FSM_VERIFY);
	if (!rc) continue;
	if (rc != CPIOERR_LSTAT_FAILED) break;

	/* XXX link(fsm->opath, fsm->path) */
	rc = fsmStage(fsm, FSM_LINK);
	if (rc) break;

#ifdef	DYING	/* XXX late commit needs to rename. */
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

/**
 */
static int fsmCommitLinks(FSM_t fsm)
{
    const char * path = fsm->path;
    const char * nsuffix = fsm->nsuffix;
    int iterIndex = fsm->ix;
    struct stat * st = &fsm->sb;
    int rc = 0;
    int i;

    fsm->path = NULL;
    fsm->nsuffix = NULL;
    fsm->ix = -1;

    for (fsm->li = fsm->links; fsm->li; fsm->li = fsm->li->next) {
	if (fsm->li->inode == st->st_ino && fsm->li->dev == st->st_dev)
	    break;
    }

    for (i = 0; i < fsm->li->nlink; i++) {
	if (fsm->li->files[i] == NULL) continue;
	fsm->ix = fsm->li->filex[i];
	rc = fsmStage(fsm, FSM_MAP);
	rc = fsmStage(fsm, FSM_COMMIT);
	fsm->path = _free(fsm->path);
	fsm->li->files[i] = _free(fsm->li->files[i]);
	fsm->li->filex[i] = -1;
    }

    fsm->ix = iterIndex;
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
    static int modulo = 4;
    const char * const cur = fileStageString(stage);
    struct stat * st = &fsm->sb;
    struct stat * ost = &fsm->osb;
    int saveerrno = errno;
    int rc = fsm->rc;
    int left;
    int i;

    if (stage & FSM_DEAD) {
	/* do nothing */
    } else if (stage & FSM_INTERNAL) {
	if (_fsm_debug && !(stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		st->st_mode, st->st_nlink, st->st_uid, st->st_gid, st->st_size,
		(fsm->path ? fsm->path : ""),
		((fsm->action != FA_UNKNOWN && fsm->action != FA_CREATE)
			? fileActionString(fsm->action) : ""));
    } else {
	fsm->stage = stage;
	if (_fsm_debug || !(stage & FSM_VERBOSE))
	    rpmMessage(RPMMESS_DEBUG, "%-8s  %06o%3d (%4d,%4d)%10d %s %s\n",
		cur,
		st->st_mode, st->st_nlink, st->st_uid, st->st_gid, st->st_size,
		(fsm->path ? fsm->path + fsm->astriplen : ""),
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
	fsm->opath = fsm->path = NULL;
	fsm->dnlx = _free(fsm->dnlx);
	fsm->ldn = _free(fsm->ldn);
	fsm->ldnalloc = fsm->ldnlen = 0;
	fsm->rdsize = 8 * BUFSIZ;
	fsm->rdbuf = fsm->rdb = _free(fsm->rdb);
	fsm->rdbuf = fsm->rdb = xmalloc(fsm->rdsize);
	fsm->wrsize = 8 * BUFSIZ;
	fsm->wrbuf = fsm->wrb = _free(fsm->wrb);
	fsm->wrbuf = fsm->wrb = xmalloc(fsm->wrsize);
	fsm->mkdirsdone = 0;
	fsm->ix = -1;
	fsm->links = NULL;
	fsm->li = NULL;
	errno = 0;	/* XXX get rid of EBADF */

#ifdef	NOTYET
	/* Detect and create directories not explicitly in package. */
	rc = fsmStage(fsm, FSM_MKDIRS);
#endif

	break;
    case FSM_INIT:
	fsm->path = _free(fsm->path);
	fsm->postpone = 0;
	fsm->diskchecked = fsm->exists = 0;
	fsm->subdir = NULL;
	fsm->suffix = (fsm->sufbuf[0] != '\0' ? fsm->sufbuf : NULL);
	fsm->action = FA_UNKNOWN;
	fsm->osuffix = NULL;
	fsm->nsuffix = NULL;

	if (fsm->goal == FSM_INSTALL) {
	    /* Detect and create directories not explicitly in package. */
	    if (!fsm->mkdirsdone) {
		rc = fsmStage(fsm, FSM_MKDIRS);
		fsm->mkdirsdone = 1;
	    }

	    /* Read next header from payload, checking for end-of-payload. */
	    rc = fsmStage(fsm, FSM_NEXT);
	}
	if (rc) break;

	/* Identify mapping index. */
	fsm->ix = ((fsm->goal == FSM_INSTALL)
		? mapFind(fsm->iter, fsm->path) : mapNextIterator(fsm->iter));

	/* On build, detect end-of-loop. */
	if (fsm->goal == FSM_BUILD && fsm->ix < 0) {
	    rc = CPIOERR_HDR_TRAILER;
	    break;
	}

	/* Generate file path. */
	rc = fsmMapPath(fsm);
	if (rc) break;

	/* Perform lstat/stat for disk file. */
	rc = fsmStage(fsm, (!(fsm->mapFlags & CPIO_FOLLOW_SYMLINKS)
			? FSM_LSTAT : FSM_STAT));
	if (rc == CPIOERR_LSTAT_FAILED && errno == ENOENT) {
	    errno = saveerrno;
	    rc = 0;
	    fsm->exists = 0;
	} else if (rc == 0) {
	    fsm->exists = 1;
	}
	fsm->diskchecked = 1;
	if (rc) break;

	/* On build, the disk file stat is what's remapped. */
	if (fsm->goal == FSM_BUILD)
	    *st = *ost;			/* structure assignment */

	rc = fsmMapAttrs(fsm);
	if (rc) break;

	fsm->postpone = XFA_SKIPPING(fsm->action);
	if (fsm->goal == FSM_INSTALL || fsm->goal == FSM_BUILD) {
	    if (!S_ISDIR(st->st_mode) && st->st_nlink > 1)
		fsm->postpone = saveHardLink(fsm);
	}
	break;
    case FSM_PRE:
	break;
    case FSM_MAP:
	rc = fsmMapPath(fsm);
	break;
    case FSM_MKDIRS:
	{   const char * path = fsm->path;
	    mode_t st_mode = st->st_mode;
	    void * dnli = dnlInitIterator(fsm, 0);
	    char * dn = fsm->rdbuf;
	    int dc = dnlCount(dnli);

	    dn[0] = '\0';
	    fsm->dnlx = (dc ? xcalloc(dc, sizeof(*fsm->dnlx)) : NULL);
	    while ((fsm->path = dnlNextIterator(dnli)) != NULL) {
		int dnlen = strlen(fsm->path);
		char * te;

		dc = dnlIndex(dnli);
		if (dc < 0) continue;
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
	    fsm->path = path;
	    st->st_mode = st_mode;		/* XXX restore st->st_mode */
	}
	break;
    case FSM_RMDIRS:
	if (fsm->dnlx) {
	    const char * path = fsm->path;
	    void * dnli = dnlInitIterator(fsm, 1);
	    char * dn = fsm->rdbuf;
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
	    fsm->path = path;
	}
	break;
    case FSM_PROCESS:
	if (fsm->postpone) {
	    if (fsm->goal == FSM_INSTALL)
		rc = fsmStage(fsm, FSM_EAT);
	    break;
	}

	if (fsm->goal == FSM_BUILD) {
	    if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
		struct hardLink * li, * prev;
		rc = writeLinkedFile(fsm);
		if (rc) break;	/* W2DO? */

		for (li = fsm->links, prev = NULL; li; prev = li, li = li->next)
		     if (li == fsm->li) break;

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

	if (fsm->goal != FSM_INSTALL)
	    break;

	if (S_ISREG(st->st_mode)) {
	    const char * path = fsm->path;
	    if (fsm->osuffix)
		fsm->path = fsmFsPath(fsm, st, NULL, NULL);
	    rc = fsmStage(fsm, FSM_VERIFY);
	    if (rc == 0 && fsm->osuffix) {
		const char * opath = fsm->opath;
		fsm->opath = fsm->path;
		fsm->path = fsmFsPath(fsm, st, NULL, fsm->osuffix);
		rc = fsmStage(fsm, FSM_RENAME);
if (!rc)
rpmMessage(RPMMESS_WARNING, _("%s saved as %s\n"), fsm->opath, fsm->path);
		fsm->path = _free(fsm->path);
		fsm->opath = opath;
	    }
	    fsm->path = path;
	    if (rc != CPIOERR_LSTAT_FAILED) return rc;
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

	    if ((st->st_size + 1) > fsm->rdsize) {
		rc = CPIOERR_HDR_SIZE;
		break;
	    }

	    fsm->wrlen = st->st_size;
	    rc = fsmStage(fsm, FSM_DREAD);
	    if (!rc && fsm->rdnb != fsm->wrlen)
		rc = CPIOERR_READ_FAILED;
	    if (rc) break;

	    fsm->wrbuf[st->st_size] = '\0';
	    /* XXX symlink(fsm->opath, fsm->path) */
	    fsm->opath = fsm->wrbuf;		/* XXX abuse fsm->path */
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
	if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
	    fsm->li->createdPath = fsm->li->linkIndex;
	    rc = fsmMakeLinks(fsm);
	}
	break;
    case FSM_POST:
	break;
    case FSM_MKLINKS:
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
	if (fsm->goal == FSM_INSTALL) {
	    (void) fsmStage(fsm,
		(S_ISDIR(st->st_mode) ? FSM_RMDIR : FSM_UNLINK));

#ifdef	NOTYET	/* XXX remove only dirs just created, not all. */
	    if (fsm->dnlx)
		(void) fsmStage(fsm, FSM_RMDIRS);
#endif
	    errno = saveerrno;
	}
	if (fsm->failedFile && *fsm->failedFile == NULL)
	    *fsm->failedFile = xstrdup(fsm->path);
	break;
    case FSM_FINI:
	if (!fsm->postpone) {
	    if (fsm->goal == FSM_INSTALL && fsm->commit)
		rc = ((!S_ISDIR(st->st_mode) && st->st_nlink > 1)
			? fsmCommitLinks(fsm) : fsmStage(fsm, FSM_COMMIT));
	}
	fsm->path = _free(fsm->path);
	fsm->opath = _free(fsm->opath);
	memset(st, 0, sizeof(*st));
	memset(ost, 0, sizeof(*ost));
	break;
    case FSM_COMMIT:
	if (!S_ISDIR(st->st_mode) && (fsm->subdir || fsm->suffix)) {
	    fsm->opath = fsm->path;
	    fsm->path = fsmFsPath(fsm, st, NULL, fsm->nsuffix);
	    rc = fsmStage(fsm, FSM_RENAME);
if (!rc && fsm->nsuffix) {
const char * opath = fsmFsPath(fsm, st, NULL, NULL);
rpmMessage(RPMMESS_WARNING, _("%s created as %s\n"), opath, fsm->path);
opath = _free(opath);
}
	    fsm->opath = _free(fsm->opath);
	}
	if (S_ISLNK(st->st_mode)) {
	    if (!rc && !getuid())
		rc = fsmStage(fsm, FSM_LCHOWN);
	} else {
	    if (!rc && !getuid())
		rc = fsmStage(fsm, FSM_CHOWN);
	    if (!rc)
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
	    if (fsm->goal == FSM_INSTALL && fsm->li->linksLeft) {
		for (i = 0 ; i < fsm->li->linksLeft; i++) {
		    if (fsm->li->filex[i] < 0) continue;
		    rc = CPIOERR_MISSING_HARDLINK;
		    if (!(fsm->li->files && fsm->li->files[i])) continue;
		    if (fsm->failedFile && *fsm->failedFile == NULL)
			*fsm->failedFile = xstrdup(fsm->li->files[i]);
		    break;
		}
	    }
	    if (fsm->goal == FSM_BUILD) {
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
	    rc = CPIOERR_LSTAT_FAILED;
	    break;
	}
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
	    return (rc ? rc : CPIOERR_LSTAT_FAILED);	/* XXX HACK */
	    /*@notreached@*/ break;
	} else if (S_ISDIR(st->st_mode)) {
	    if (S_ISDIR(ost->st_mode))		return 0;
	    if (S_ISLNK(ost->st_mode)) {
		rc = fsmStage(fsm, FSM_STAT);
		if (rc == CPIOERR_STAT_FAILED && errno == ENOENT) rc = 0;
		if (rc) break;
		errno = saveerrno;
		if (S_ISDIR(ost->st_mode))	return 0;
	    }
	} else if (S_ISLNK(st->st_mode)) {
	    if (S_ISLNK(ost->st_mode)) {
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
		rc = fsmStage(fsm, FSM_READLINK);
		errno = saveerrno;
		if (rc) break;
		if (!strcmp(fsm->opath, fsm->rdbuf))	return 0;
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
	/*@notreached@*/ break;

    case FSM_UNLINK:
	rc = Unlink(fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_UNLINK_FAILED;
	break;
    case FSM_RENAME:
	rc = Rename(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_RENAME_FAILED;
	break;
    case FSM_MKDIR:
	rc = Mkdir(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKDIR_FAILED;
	break;
    case FSM_RMDIR:
	rc = Rmdir(fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_RMDIR_FAILED;
	break;
    case FSM_CHOWN:
	rc = chown(fsm->path, st->st_uid, st->st_gid);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, %d) %s\n", cur,
		fsm->path, st->st_uid, st->st_gid,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
	break;
    case FSM_LCHOWN:
#if ! CHOWN_FOLLOWS_SYMLINK
	rc = lchown(fsm->path, st->st_uid, st->st_gid);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, %d) %s\n", cur,
		fsm->path, st->st_uid, st->st_gid,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHOWN_FAILED;
#endif
	break;
    case FSM_CHMOD:
	rc = chmod(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_CHMOD_FAILED;
	break;
    case FSM_UTIME:
	{   struct utimbuf stamp;
	    stamp.actime = st->st_mtime;
	    stamp.modtime = st->st_mtime;
	    rc = utime(fsm->path, &stamp);
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0x%x) %s\n", cur,
			fsm->path, (unsigned)st->st_mtime,
			(rc < 0 ? strerror(errno) : ""));
	    if (rc < 0)	rc = CPIOERR_UTIME_FAILED;
	}
	break;
    case FSM_SYMLINK:
	rc = symlink(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_SYMLINK_FAILED;
	break;
    case FSM_LINK:
	rc = Link(fsm->opath, fsm->path);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %s) %s\n", cur,
		fsm->opath, fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_LINK_FAILED;
	break;
    case FSM_MKFIFO:
	rc = mkfifo(fsm->path, (st->st_mode & 07777));
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%04o) %s\n", cur,
		fsm->path, (st->st_mode & 07777),
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKFIFO_FAILED;
	break;
    case FSM_MKNOD:
	/*@-unrecog@*/
	rc = mknod(fsm->path, (st->st_mode & ~07777), st->st_rdev);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, 0%o, 0x%x) %s\n", cur,
		fsm->path, (st->st_mode & ~07777), (unsigned)st->st_rdev,
		(rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_MKNOD_FAILED;
	/*@=unrecog@*/
	break;
    case FSM_LSTAT:
	rc = Lstat(fsm->path, ost);
	if (_fsm_debug && (stage & FSM_SYSCALL) && rc && errno != ENOENT)
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, ost) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_LSTAT_FAILED;
	break;
    case FSM_STAT:
	rc = Stat(fsm->path, ost);
	if (_fsm_debug && (stage & FSM_SYSCALL) && rc && errno != ENOENT)
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, ost) %s\n", cur,
		fsm->path, (rc < 0 ? strerror(errno) : ""));
	if (rc < 0)	rc = CPIOERR_STAT_FAILED;
	break;
    case FSM_READLINK:
	/* XXX NUL terminated result in fsm->rdbuf, len in fsm->rdnb. */
	rc = Readlink(fsm->path, fsm->rdbuf, fsm->rdsize - 1);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, rdbuf, %d) %s\n", cur,
		fsm->path, fsm->rdlen, (rc < 0 ? strerror(errno) : ""));
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
	rc = fsmStage(fsm, FSM_HREAD);
	if (rc) break;
	if (!strcmp(fsm->path, CPIO_TRAILER)) { /* Detect end-of-payload. */
	    fsm->path = _free(fsm->path);
	    rc = CPIOERR_HDR_TRAILER;
	}
	if (!rc)
	    rc = fsmStage(fsm, FSM_POS);
	break;
    case FSM_EAT:
	for (left = st->st_size; left > 0; left -= fsm->rdnb) {
	    fsm->wrlen = (left > fsm->wrsize ? fsm->wrsize : left);
	    rc = fsmStage(fsm, FSM_DREAD);
	    if (rc) break;
	}
	break;
    case FSM_POS:
	left = (modulo - (fdGetCpioPos(fsm->cfd) % modulo)) % modulo;
	if (left) {
	    fsm->wrlen = left;
	    (void) fsmStage(fsm, FSM_DREAD);
	}
	break;
    case FSM_PAD:
	left = (modulo - (fdGetCpioPos(fsm->cfd) % modulo)) % modulo;
	if (left) {
	    memset(fsm->rdbuf, 0, left);
	    /* XXX DWRITE uses rdnb for I/O length. */
	    fsm->rdnb = left;
	    (void) fsmStage(fsm, FSM_DWRITE);
	}
	break;
    case FSM_TRAILER:
	rc = cpioTrailerWrite(fsm);
	break;
    case FSM_HREAD:
	rc = fsmStage(fsm, FSM_POS);
	if (!rc)
	    rc = cpioHeaderRead(fsm, st);	/* Read next payload header. */
	break;
    case FSM_HWRITE:
	rc = cpioHeaderWrite(fsm, st);		/* Write next payload header. */
	break;
    case FSM_DREAD:
	fsm->rdnb = Fread(fsm->wrbuf, sizeof(*fsm->wrbuf), fsm->wrlen, fsm->cfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, cfd)\trdnb %d\n",
		cur, (fsm->wrbuf == fsm->wrb ? "wrbuf" : "mmap"),
		fsm->wrlen, fsm->rdnb);
if (fsm->rdnb != fsm->wrlen) fprintf(stderr, "*** short read, had %d, got %d\n", (int)fsm->rdnb, (int)fsm->wrlen);
#ifdef	NOTYET
	if (Ferror(fsm->rfd))
	    rc = CPIOERR_READ_FAILED;
#endif
	if (fsm->rdnb > 0)
	    fdSetCpioPos(fsm->cfd, fdGetCpioPos(fsm->cfd) + fsm->rdnb);
	break;
    case FSM_DWRITE:
	fsm->wrnb = Fwrite(fsm->rdbuf, sizeof(*fsm->rdbuf), fsm->rdnb, fsm->cfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, %d, cfd)\twrnb %d\n",
		cur, (fsm->rdbuf == fsm->rdb ? "rdbuf" : "mmap"),
		fsm->rdnb, fsm->wrnb);
if (fsm->rdnb != fsm->wrnb) fprintf(stderr, "*** short write, had %d, got %d\n", (int)fsm->rdnb, (int)fsm->wrnb);
#ifdef	NOTYET
	if (Ferror(fsm->wfd))
	    rc = CPIOERR_WRITE_FAILED;
#endif
	if (fsm->wrnb > 0)
	    fdSetCpioPos(fsm->cfd, fdGetCpioPos(fsm->cfd) + fsm->wrnb);
	break;

    case FSM_ROPEN:
	fsm->rfd = Fopen(fsm->path, "r.ufdio");
	if (fsm->rfd == NULL || Ferror(fsm->rfd)) {
	    if (fsm->rfd)	(void) fsmStage(fsm, FSM_RCLOSE);
	    fsm->rfd = NULL;
	    rc = CPIOERR_OPEN_FAILED;
	    break;
	}
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, \"r\") rfd %p rdbuf %p\n", cur,
		fsm->path, fsm->rfd, fsm->rdbuf);
	break;
    case FSM_READ:
	fsm->rdnb = Fread(fsm->rdbuf, sizeof(*fsm->rdbuf), fsm->rdlen, fsm->rfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (rdbuf, %d, rfd)\trdnb %d\n",
		cur, fsm->rdlen, fsm->rdnb);
if (fsm->rdnb != fsm->rdlen) fprintf(stderr, "*** short read, had %d, got %d\n", (int)fsm->rdnb, (int)fsm->rdlen);
#ifdef	NOTYET
	if (Ferror(fsm->rfd))
	    rc = CPIOERR_READ_FAILED;
#endif
	break;
    case FSM_RCLOSE:
	if (fsm->rfd) {
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmMessage(RPMMESS_DEBUG, " %8s (%p)\n", cur, fsm->rfd);
	    (void) Fclose(fsm->rfd);
	    errno = saveerrno;
	}
	fsm->rfd = NULL;
	break;
    case FSM_WOPEN:
	fsm->wfd = Fopen(fsm->path, "w.ufdio");
	if (fsm->wfd == NULL || Ferror(fsm->wfd)) {
	    if (fsm->wfd)	(void) fsmStage(fsm, FSM_WCLOSE);
	    fsm->wfd = NULL;
	    rc = CPIOERR_OPEN_FAILED;
	}
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (%s, \"w\") wfd %p wrbuf %p\n", cur,
		fsm->path, fsm->wfd, fsm->wrbuf);
	break;
    case FSM_WRITE:
	fsm->wrnb = Fwrite(fsm->wrbuf, sizeof(*fsm->wrbuf), fsm->rdnb, fsm->wfd);
	if (_fsm_debug && (stage & FSM_SYSCALL))
	    rpmMessage(RPMMESS_DEBUG, " %8s (wrbuf, %d, wfd)\twrnb %d\n",
		cur, fsm->rdnb, fsm->wrnb);
if (fsm->rdnb != fsm->wrnb) fprintf(stderr, "*** short write: had %d, got %d\n", (int)fsm->rdnb, (int)fsm->wrnb);
#ifdef	NOTYET
	if (Ferror(fsm->wfd))
	    rc = CPIOERR_WRITE_FAILED;
#endif
	break;
    case FSM_WCLOSE:
	if (fsm->wfd) {
	    if (_fsm_debug && (stage & FSM_SYSCALL))
		rpmMessage(RPMMESS_DEBUG, " %8s (%p)\n", cur, fsm->wfd);
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

/** @todo Verify payload MD5 sum. */
int cpioInstallArchive(FSM_t fsm)
{
    int rc = 0;

#ifdef	NOTYET
    char * md5sum = NULL;

    fdInitMD5(cfd, 0);
#endif

    while (1) {

	/* Clean fsm, free'ing memory. Read next archive header. */
	rc = fsmStage(fsm, FSM_INIT);

	/* Exit on end-of-payload. */
	if (rc == CPIOERR_HDR_TRAILER) {
	    rc = 0;
	    break;
	}
	if (rc) {
	    fsm->postpone = 1;
	    (void) fsmStage(fsm, FSM_UNDO);
	    goto exit;
	}

	/* Extract file from archive. */
	rc = fsmStage(fsm, FSM_PROCESS);
	if (rc) {
	    (void) fsmStage(fsm, FSM_UNDO);
	    goto exit;
	}

	/* Notify on success. */
	(void) fsmStage(fsm, FSM_NOTIFY);

	(void) fsmStage(fsm, FSM_FINI);

    }

#ifdef	NOTYET
    fdFiniMD5(fsm->cfd, (void **)&md5sum, NULL, 1);

    if (md5sum)
	free(md5sum);
#endif

    rc = fsmStage(fsm, FSM_DESTROY);

exit:
    return rc;
}

int cpioBuildArchive(FSM_t fsm)
{
    size_t pos = fdGetCpioPos(fsm->cfd);
    int rc = 0;

    while (1) {

	rc = fsmStage(fsm, FSM_INIT);

	if (rc == CPIOERR_HDR_TRAILER) {
	    rc = 0;
	    break;
	}
	if (rc) {
	    fsm->postpone = 1;
	    (void) fsmStage(fsm, FSM_UNDO);
	    goto exit;
	}

	rc = fsmStage(fsm, FSM_PROCESS);
	if (rc) {
	    (void) fsmStage(fsm, FSM_UNDO);
	    goto exit;
	}
	(void) fsmStage(fsm, FSM_FINI);
    }

    if (!rc)
	rc = fsmStage(fsm, FSM_TRAILER);

    if (!rc && fsm->archiveSize)
	*fsm->archiveSize = (fdGetCpioPos(fsm->cfd) - pos);

    rc = fsmStage(fsm, FSM_DESTROY);

exit:
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
