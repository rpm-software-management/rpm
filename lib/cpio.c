#include "system.h"

#include "cpio.h"

#define CPIO_NEWC_MAGIC	"070701"
#define CPIO_CRC_MAGIC	"070702"
#define TRAILER		"TRAILER!!!"

/* FIXME: We don't translate between cpio and system mode bits! These
   should both be the same, but really odd things are going to happen if
   that's not true! */

struct hardLink {
    struct hardLink * next;
    char ** files;		/* there are nlink of these, used by install */
    int * fileMaps;		/* used by build */
    dev_t dev;
    ino_t inode;
    int nlink;			
    int linksLeft;
    int createdPath;
    struct stat sb;
};

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

#define	PHYS_HDR_SIZE	110		/* don't depend on sizeof(struct) */

struct cpioHeader {
    ino_t inode;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int nlink;
    time_t mtime;
    long size;
    dev_t dev, rdev;
    char * path;
};

static inline off_t saferead(CFD_t *cfd, void * vbuf, size_t amount) {
    off_t rc = 0;
    char * buf = vbuf;

    while (amount > 0) {
	size_t nb;
	switch (cfd->cpioIoType) {
	default:
#ifdef	PARANOID
		fprintf(stderr, "\tsaferead(%p,%p,%x)\n", cfd, vbuf, (unsigned)amount);
		exit(1);
		break;
#endif
	case cpioIoTypeDebug:
		nb = amount;
		fprintf(stderr, "\tsaferead(%p,%p,%x)\n", cfd, vbuf, (unsigned)amount);
		break;
	case cpioIoTypeFd:
		nb = fdRead(cfd->cpioFd, buf, amount);
		break;
	case cpioIoTypeFp:
		nb = fread(buf, amount, 1, cfd->cpioFp);
		if (nb)
			nb *= amount;
		break;
	case cpioIoTypeGzFd:
		nb = gzdRead(cfd->cpioGzFd, buf, amount);
		break;
	}
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

static inline off_t ourread(CFD_t * cfd, void * buf, size_t size) {
    off_t i = saferead(cfd, buf, size);
    if (i > 0)
	cfd->cpioPos += i;
    return i;
}

static inline void padinfd(CFD_t * cfd, int modulo) {
    int buf[10];
    int amount;
    
    amount = (modulo - cfd->cpioPos % modulo) % modulo;
    (void)ourread(cfd, buf, amount);
}

static inline off_t safewrite(CFD_t *cfd, void * vbuf, size_t amount) {
    off_t rc = 0;
    char * buf = vbuf;

    while (amount > 0) {
	size_t nb;
	switch (cfd->cpioIoType) {
	default:
#ifdef	PARANOID
		fprintf(stderr, "\tsafewrite(%p,%p,%x)\n", cfd, vbuf, (unsigned)amount);
		exit(1);
		break;
#endif
	case cpioIoTypeDebug:
		nb = amount;
		fprintf(stderr, "\tsafewrite(%p,%p,%x)\n", cfd, vbuf, (unsigned)amount);
		break;
	case cpioIoTypeFd:
		nb = fdWrite(cfd->cpioFd, buf, amount);
		break;
	case cpioIoTypeFp:
		nb = fwrite(buf, amount, 1, cfd->cpioFp);
		if (nb)
			nb *= amount;
		break;
	case cpioIoTypeGzFd:
		nb = gzdWrite(cfd->cpioGzFd, buf, amount);
		break;
	}
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

static inline int padoutfd(CFD_t * cfd, size_t * where, int modulo) {
    static int buf[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int amount;
    
    amount = (modulo - *where % modulo) % modulo;
    *where += amount;

    if (safewrite(cfd, buf, amount) != amount) 
	return CPIOERR_WRITE_FAILED;
    return 0;
}

static int strntoul(const char * str, char ** endptr, int base, int num) {
    char * buf, * end;
    unsigned long ret;

    buf = alloca(num + 1);
    strncpy(buf, str, num);
    buf[num] = '\0';

    ret = strtoul(buf, &end, base);
    if (*end)
	*endptr = ((char *)str) + (end - buf);	/* XXX discards const */
    else
	*endptr = "";

    return strtoul(buf, endptr, base);
}

#define GET_NUM_FIELD(phys, log) \
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	if (*end) return CPIOERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	memcpy(phys, space, 8);

static int getNextHeader(CFD_t * cfd, struct cpioHeader * chPtr) {
    struct cpioCrcPhysicalHeader physHeader;
    int nameSize;
    char * end;
    int major, minor;

    if (ourread(cfd, &physHeader, PHYS_HDR_SIZE) != PHYS_HDR_SIZE) 
	return CPIOERR_READ_FAILED;

    if (strncmp(CPIO_CRC_MAGIC, physHeader.magic, strlen(CPIO_CRC_MAGIC)) &&
	strncmp(CPIO_NEWC_MAGIC, physHeader.magic, strlen(CPIO_NEWC_MAGIC)))
	return CPIOERR_BAD_MAGIC;

    GET_NUM_FIELD(physHeader.inode, chPtr->inode);
    GET_NUM_FIELD(physHeader.mode, chPtr->mode);
    GET_NUM_FIELD(physHeader.uid, chPtr->uid);
    GET_NUM_FIELD(physHeader.gid, chPtr->gid);
    GET_NUM_FIELD(physHeader.nlink, chPtr->nlink);
    GET_NUM_FIELD(physHeader.mtime, chPtr->mtime);
    GET_NUM_FIELD(physHeader.filesize, chPtr->size);

    GET_NUM_FIELD(physHeader.devMajor, major);
    GET_NUM_FIELD(physHeader.devMinor, minor);
#ifndef __LCLINT__
    chPtr->dev = makedev(major, minor);
#endif

    GET_NUM_FIELD(physHeader.rdevMajor, major);
    GET_NUM_FIELD(physHeader.rdevMinor, minor);
#ifndef __LCLINT__
    chPtr->rdev = makedev(major, minor);
#endif

    GET_NUM_FIELD(physHeader.namesize, nameSize);

    chPtr->path = malloc(nameSize + 1);
    if (ourread(cfd, chPtr->path, nameSize) != nameSize) {
	free(chPtr->path);
	return CPIOERR_BAD_HEADER;
    }

    /* this is unecessary chPtr->path[nameSize] = '\0'; */

    padinfd(cfd, 4);

    return 0;
}

int cpioFileMapCmp(const void * a, const void * b) {
    const struct cpioFileMapping * first = a;
    const struct cpioFileMapping * second = b;

    return (strcmp(first->archivePath, second->archivePath));
}

/* This could trash files in the path! I'm not sure that's a good thing */
static int createDirectory(char * path, mode_t perms) {
    struct stat sb;

    if (!lstat(path, &sb)) {
	int dounlink = 0;	/* XXX eliminate, dounlink==1 on all paths */
	if (S_ISDIR(sb.st_mode)) {
	    return 0;
	} else if (S_ISLNK(sb.st_mode)) {
	    if (stat(path, &sb)) {
		if (errno != ENOENT) 
		    return CPIOERR_STAT_FAILED;
		dounlink = 1;
	    } else {
		if (S_ISDIR(sb.st_mode))
		    return 0;
		dounlink = 1;
	    }
	} else {
	    dounlink = 1;
	}

	if (dounlink && unlink(path)) {
	    return CPIOERR_UNLINK_FAILED;
	}
    }

    if (mkdir(path, 000))
	return CPIOERR_MKDIR_FAILED;

    if (chmod(path, perms))
	return CPIOERR_CHMOD_FAILED;

    return 0;
}

static int setInfo(struct cpioHeader * hdr) {
    int rc = 0;
    struct utimbuf stamp;

    stamp.actime = hdr->mtime; 
    stamp.modtime = hdr->mtime;

    if (!S_ISLNK(hdr->mode)) {
	if (!getuid() && chown(hdr->path, hdr->uid, hdr->gid))
	    rc = CPIOERR_CHOWN_FAILED;
	if (!rc && chmod(hdr->path, hdr->mode & 07777))
	    rc = CPIOERR_CHMOD_FAILED;
	if (!rc && utime(hdr->path, &stamp))
	    rc = CPIOERR_UTIME_FAILED;
    } else {
#       if ! CHOWN_FOLLOWS_SYMLINK
	    if (!getuid() && !rc && lchown(hdr->path, hdr->uid, hdr->gid))
		rc = CPIOERR_CHOWN_FAILED;
#       endif
    }

    return rc;
}

static int checkDirectory(char * filename) {
    static char * lastDir = NULL;
    static int lastDirLength = 0;
    static int lastDirAlloced = 0;
    int length = strlen(filename);
    char * buf;
    char * chptr;
    int rc = 0;

    buf = alloca(length + 1);
    strcpy(buf, filename);

    for (chptr = buf + length - 1; chptr > buf; chptr--) {
	if (*chptr == '/') break;
    }

    if (chptr == buf) return 0;     /* /filename - no directories */

    *chptr = '\0';                  /* buffer is now just directories */

    length = strlen(buf);
    if (lastDirLength == length && !strcmp(buf, lastDir)) return 0;

    if (lastDirAlloced < (length + 1)) {
	lastDirAlloced = length + 100;
	lastDir = realloc(lastDir, lastDirAlloced);
    }

    strcpy(lastDir, buf);
    lastDirLength = length;

    for (chptr = buf + 1; *chptr; chptr++) {
	if (*chptr == '/') {
	    *chptr = '\0';
	    rc = createDirectory(buf, 0755);
	    *chptr = '/';
	    if (rc) return rc;
	}
    }
    rc = createDirectory(buf, 0755);

    return rc;
}

static int expandRegular(CFD_t * cfd, struct cpioHeader * hdr,
			 cpioCallback cb, void * cbData) {
    FD_t out;
    char buf[8192];
    int bytesRead;
    int left = hdr->size;
    int rc = 0;
    struct cpioCallbackInfo cbInfo;
    struct stat sb;

    if (!lstat(hdr->path, &sb))
	if (unlink(hdr->path))
	    return CPIOERR_UNLINK_FAILED;

    out = fdOpen(hdr->path, O_CREAT | O_WRONLY, 0);
    if (fdFileno(out) < 0) 
	return CPIOERR_OPEN_FAILED;

    cbInfo.file = hdr->path;
    cbInfo.fileSize = hdr->size;

    while (left) {
	bytesRead = ourread(cfd, buf, left < sizeof(buf) ? left : sizeof(buf));
	if (bytesRead <= 0) {
	    rc = CPIOERR_READ_FAILED;
	    break;
	}

	if (fdWrite(out, buf, bytesRead) != bytesRead) {
	    rc = CPIOERR_COPY_FAILED;
	    break;
	}

	left -= bytesRead;

	/* don't call this with fileSize == fileComplete */
	if (!rc && cb && left) {
	    cbInfo.fileComplete = hdr->size - left;
	    cbInfo.bytesProcessed = cfd->cpioPos;
	    cb(&cbInfo, cbData);
	}
    }

    fdClose(out);
    
    return rc;
}

static int expandSymlink(CFD_t * cfd, struct cpioHeader * hdr) {
    char buf[2048], buf2[2048];
    struct stat sb;
    int len;

    if ((hdr->size + 1)> sizeof(buf))
	return CPIOERR_HDR_SIZE;

    if (ourread(cfd, buf, hdr->size) != hdr->size)
	return CPIOERR_READ_FAILED;

    buf[hdr->size] = '\0';

    if (!lstat(hdr->path, &sb)) {
	if (S_ISLNK(sb.st_mode)) {
	    len = readlink(hdr->path, buf2, sizeof(buf2) - 1);
	    if (len > 0) {
		buf2[len] = '\0';
		if (!strcmp(buf, buf2)) return 0;
	    }
	}

	if (unlink(hdr->path))
	    return CPIOERR_UNLINK_FAILED;
    }

    if (symlink(buf, hdr->path) < 0)
	return CPIOERR_SYMLINK_FAILED;

    return 0;
}

static int expandFifo(CFD_t * cfd, struct cpioHeader * hdr) {
    struct stat sb;

    if (!lstat(hdr->path, &sb)) {
	if (S_ISFIFO(sb.st_mode)) return 0;

	if (unlink(hdr->path))
	    return CPIOERR_UNLINK_FAILED;
    }

    if (mkfifo(hdr->path, 0))
	return CPIOERR_MKFIFO_FAILED;

    return 0; 
}

static int expandDevice(CFD_t * cfd, struct cpioHeader * hdr) {
    struct stat sb;

    if (!lstat(hdr->path, &sb)) {
	if ((S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode)) && 
		(sb.st_rdev == hdr->rdev))
	    return 0;
	if (unlink(hdr->path))
	    return CPIOERR_UNLINK_FAILED;
    }

    if (mknod(hdr->path, hdr->mode & (~0777), hdr->rdev))
	return CPIOERR_MKNOD_FAILED;
    
    return 0;
}

static void freeLink(struct hardLink * li) {
    int i;

    for (i = 0; i < li->nlink; i++) {
	if (li->files[i]) free(li->files[i]);
    }
    free(li->files);
}

static int createLinks(struct hardLink * li, char ** failedFile) {
    int i;
    struct stat sb;

    for (i = 0; i < li->nlink; i++) {
	if (i == li->createdPath) continue;
	if (!li->files[i]) continue;

	if (!lstat(li->files[i], &sb)) {
	    if (unlink(li->files[i])) {
		*failedFile = strdup(li->files[i]);
		return CPIOERR_UNLINK_FAILED;
	    }
	}

	if (link(li->files[li->createdPath], li->files[i])) {
	    *failedFile = strdup(li->files[i]);
	    return CPIOERR_LINK_FAILED;
	}

	free(li->files[i]);
	li->files[i] = NULL;
	li->linksLeft--;
    }

    return 0;
}

static int eatBytes(CFD_t * cfd, int amount) {
    char buf[4096];
    int bite;
   
    while (amount) {
	bite = (amount > sizeof(buf)) ? sizeof(buf) : amount;
	if (ourread(cfd, buf, bite) != bite)
	    return CPIOERR_READ_FAILED;
	amount -= bite;
    }

    return 0;
}

int cpioInstallArchive(CFD_t *cfd, struct cpioFileMapping * mappings, 
		       int numMappings, cpioCallback cb, void * cbData,
		       char ** failedFile) {
    struct cpioHeader ch;
    int rc = 0;
    int linkNum = 0;
    struct cpioFileMapping * map = NULL;
    struct cpioFileMapping needle;
    mode_t cpioMode;
    int olderr;
    struct cpioCallbackInfo cbInfo;
    struct hardLink * links = NULL;
    struct hardLink * li = NULL;

    cfd->cpioPos = 0;
    *failedFile = NULL;

    ch.path = NULL;
    do {
	if ((rc = getNextHeader(cfd, &ch))) {
#if 0	/* XXX this is the failure point for an unreadable rpm */
	    fprintf(stderr, _("getNextHeader: %s\n"), cpioStrerror(rc));
#endif
	    return rc;
	}

	if (!strcmp(ch.path, TRAILER)) {
	    free(ch.path);
	    break;
	}

	if (mappings) {
	    needle.archivePath = ch.path;
	    map = bsearch(&needle, mappings, numMappings, sizeof(needle),
			  cpioFileMapCmp);
	}

	if (mappings && !map) {
	    eatBytes(cfd, ch.size);
	} else {
	    cpioMode = ch.mode;

	    if (map) {
		if (map->mapFlags & CPIO_MAP_PATH) {
		    free(ch.path);
		    ch.path = strdup(map->fsPath);
		} 

		if (map->mapFlags & CPIO_MAP_MODE)
		    ch.mode = map->finalMode;
		if (map->mapFlags & CPIO_MAP_UID)
		    ch.uid = map->finalUid;
		if (map->mapFlags & CPIO_MAP_GID)
		    ch.gid = map->finalGid;
	    }

	    /* This won't get hard linked symlinks right, but I can't seem 
	       to create those anyway */

	    if (S_ISREG(ch.mode) && ch.nlink > 1) {
		li = links;
		for (li = links; li; li = li->next) {
		    if (li->inode == ch.inode && li->dev == ch.dev) break;
		}

		if (!li) {
		    li = malloc(sizeof(*li));
		    li->inode = ch.inode;
		    li->dev = ch.dev;
		    li->nlink = ch.nlink;
		    li->linksLeft = ch.nlink;
		    li->createdPath = -1;
		    li->files = calloc(sizeof(char *), li->nlink);
		    li->next = links;
		    links = li;
		}

		for (linkNum = 0; linkNum < li->nlink; linkNum++)
		    if (!li->files[linkNum]) break;
		li->files[linkNum] = strdup(ch.path);
	    }
		
	    if ((ch.nlink > 1) && S_ISREG(ch.mode) && !ch.size &&
		li->createdPath == -1) {
		/* defer file creation */
	    } else if ((ch.nlink > 1) && S_ISREG(ch.mode) && 
		       (li->createdPath != -1)) {
		createLinks(li, failedFile);

		/* this only happens for cpio archives which contain
		   hardlinks w/ the contents of each hardlink being
		   listed (intead of the data being given just once. This
		   shouldn't happen, but I've made it happen w/ buggy
		   code, so what the heck? GNU cpio handles this well fwiw */
		if (ch.size) eatBytes(cfd, ch.size);
	    } else {
		rc = checkDirectory(ch.path);

		if (!rc) {
		    if (S_ISREG(ch.mode))
			rc = expandRegular(cfd, &ch, cb, cbData);
		    else if (S_ISDIR(ch.mode))
			rc = createDirectory(ch.path, 000);
		    else if (S_ISLNK(ch.mode))
			rc = expandSymlink(cfd, &ch);
		    else if (S_ISFIFO(ch.mode))
			rc = expandFifo(cfd, &ch);
		    else if (S_ISCHR(ch.mode) || S_ISBLK(ch.mode))
			rc = expandDevice(cfd, &ch);
		    else if (S_ISSOCK(ch.mode)) {
			/* this mimicks cpio but probably isnt' right */
			rc = expandFifo(cfd, &ch);
		    } else {
			rc = CPIOERR_INTERNAL;
		    }
		}

		if (!rc)
		    rc = setInfo(&ch);

		if (S_ISREG(ch.mode) && ch.nlink > 1) {
		    li->createdPath = linkNum;
		    li->linksLeft--;
		    rc = createLinks(li, failedFile);
		}
	    }

	    if (rc && !*failedFile) {
		*failedFile = strdup(ch.path);

		olderr = errno;
		unlink(ch.path);
		errno = olderr;
	    }
	}

	padinfd(cfd, 4);

	if (!rc && cb) {
	    cbInfo.file = ch.path;
	    cbInfo.fileSize = ch.size;
	    cbInfo.fileComplete = ch.size;
	    cbInfo.bytesProcessed = cfd->cpioPos;
	    cb(&cbInfo, cbData);
	}

	free(ch.path);
	ch.path = NULL;
    } while (1 && !rc);

    li = links;
    while (li && !rc) {
	if (li->linksLeft) {
	    if (li->createdPath == -1)
		rc = CPIOERR_INTERNAL;
	    else 
		rc = createLinks(li, failedFile);
	}

	freeLink(li);

	links = li;
	li = li->next;
	free(links);
	links = li;
    }

    li = links;
    /* if an error got us here links will still be eating some memory */
    while (li) {
	freeLink(li);
	links = li;
	li = li->next;
	free(links);
    }

    return rc;
}

static int writeFile(CFD_t *cfd, struct stat sb, struct cpioFileMapping * map, 
		     size_t * sizeptr, int writeData) {
    struct cpioCrcPhysicalHeader hdr;
    char buf[8192], symbuf[2048];
    dev_t num;
    FD_t datafd;
    size_t size, amount = 0;
    int rc, olderrno;

    if (!(map->mapFlags & CPIO_MAP_PATH))
	map->archivePath = map->fsPath;
    if (map->mapFlags & CPIO_MAP_MODE)
	sb.st_mode = (sb.st_mode & S_IFMT) | map->finalMode;
    if (map->mapFlags & CPIO_MAP_UID)
	sb.st_uid = map->finalUid;
    if (map->mapFlags & CPIO_MAP_GID)
	sb.st_gid = map->finalGid;

    if (!writeData || S_ISDIR(sb.st_mode)) {
	sb.st_size = 0;
    } else if (S_ISLNK(sb.st_mode)) {
	/* While linux puts the size of a symlink in the st_size field,
	   I don't think that's a specified standard */

	amount = readlink(map->fsPath, symbuf, sizeof(symbuf));
	if (amount <= 0) {
	    return CPIOERR_READLINK_FAILED;
	}

	sb.st_size = amount;
    }

    memcpy(hdr.magic, CPIO_NEWC_MAGIC, sizeof(hdr.magic));
    SET_NUM_FIELD(hdr.inode, sb.st_ino, buf);
    SET_NUM_FIELD(hdr.mode, sb.st_mode, buf);
    SET_NUM_FIELD(hdr.uid, sb.st_uid, buf);
    SET_NUM_FIELD(hdr.gid, sb.st_gid, buf);
    SET_NUM_FIELD(hdr.nlink, sb.st_nlink, buf);
    SET_NUM_FIELD(hdr.mtime, sb.st_mtime, buf);
    SET_NUM_FIELD(hdr.filesize, sb.st_size, buf);

    num = major(sb.st_dev); SET_NUM_FIELD(hdr.devMajor, num, buf);
    num = minor(sb.st_dev); SET_NUM_FIELD(hdr.devMinor, num, buf);
    num = major(sb.st_rdev); SET_NUM_FIELD(hdr.rdevMajor, num, buf);
    num = minor(sb.st_rdev); SET_NUM_FIELD(hdr.rdevMinor, num, buf);

    num = strlen(map->archivePath) + 1; SET_NUM_FIELD(hdr.namesize, num, buf);
    memcpy(hdr.checksum, "00000000", 8);

    if ((rc = safewrite(cfd, &hdr, PHYS_HDR_SIZE)) != PHYS_HDR_SIZE)
	return rc;
    if ((rc = safewrite(cfd, map->archivePath, num)) != num)
	return rc;
    size = PHYS_HDR_SIZE + num;
    if ((rc = padoutfd(cfd, &size, 4)))
	return rc;
	
    if (writeData && S_ISREG(sb.st_mode)) {
	char *b;
#if HAVE_MMAP
	void *mapped;
	size_t nmapped;
#endif

	if (fdFileno(datafd = fdOpen(map->fsPath, O_RDONLY, 0)) < 0)
	    return CPIOERR_OPEN_FAILED;

#if HAVE_MMAP
	nmapped = 0;
	mapped = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fdFileno(datafd), 0);
	if (mapped != (void *)-1) {
	    b = (char *)mapped;
	    nmapped = sb.st_size;
	} else
#endif
	{
	    b = buf;
	}

	size += sb.st_size;

	while (sb.st_size) {
#if HAVE_MMAP
	  if (mapped != (void *)-1) {
	    amount = nmapped;
	  } else
#endif
	  {
	    amount = fdRead(datafd, b, 
		     sb.st_size > sizeof(buf) ? sizeof(buf) : sb.st_size);
	    if (amount <= 0) {
		olderrno = errno;
		fdClose(datafd);
		errno = olderrno;
		return CPIOERR_READ_FAILED;
	    }
	  }

	    if ((rc = safewrite(cfd, b, amount)) != amount) {
		olderrno = errno;
		fdClose(datafd);
		errno = olderrno;
		return rc;
	    }

	    sb.st_size -= amount;
	}

#if HAVE_MMAP
	if (mapped != (void *)-1) {
	    munmap(mapped, nmapped);
	}
#endif

	fdClose(datafd);
    } else if (writeData && S_ISLNK(sb.st_mode)) {
	if ((rc = safewrite(cfd, symbuf, amount)) != amount)
	    return rc;
	size += amount;
    }

    /* this is a noop for most file types */
    if ((rc = padoutfd(cfd, &size, 4)))
	return rc;

    *sizeptr = size;

    return 0;
}

static int writeLinkedFile(CFD_t *cfd, struct hardLink * hlink, 
			   struct cpioFileMapping * mappings,
			   cpioCallback cb, void * cbData,
			   size_t * sizeptr,
			   char ** failedFile) {
    int i, rc;
    size_t size;
    struct cpioCallbackInfo cbinfo;

    *sizeptr = 0;

    for (i = hlink->nlink - 1; i > hlink->linksLeft; i--) {
	if ((rc = writeFile(cfd, hlink->sb, mappings + hlink->fileMaps[i], 
			    &size, 0))) {
	    if (failedFile) *failedFile = 
		    mappings[hlink->fileMaps[i]].fsPath;
	    return rc;
	}

	*sizeptr += size;

	if (cb) {
	    cbinfo.file = mappings[i].archivePath;
	    cb(&cbinfo, cbData);
	}
    }

    if ((rc = writeFile(cfd, hlink->sb, 
			mappings + hlink->fileMaps[hlink->linksLeft], 
			&size, 1))) {
	if (failedFile) 
	    *failedFile = mappings[hlink->fileMaps[hlink->linksLeft]].fsPath;
	return rc;
    }

    *sizeptr += size;

    if (cb) {
	cbinfo.file = mappings[i].archivePath;
	cb(&cbinfo, cbData);
    }

    return 0;
}

int cpioBuildArchive(CFD_t *cfd, struct cpioFileMapping * mappings, 
		     int numMappings, cpioCallback cb, void * cbData,
		     unsigned int * archiveSize, char ** failedFile) {
    size_t size, totalsize = 0;
    int rc;
    int i;
    struct cpioCallbackInfo cbinfo;
    struct cpioCrcPhysicalHeader hdr;
    struct stat sb;
/*@-fullinitblock@*/
    struct hardLink hlinkList = { NULL };
/*@=fullinitblock@*/
    struct hardLink * hlink, * parent;

    hlinkList.next = NULL;

    for (i = 0; i < numMappings; i++) {
	if (mappings[i].mapFlags & CPIO_FOLLOW_SYMLINKS)
	    rc = stat(mappings[i].fsPath, &sb);
	else
	    rc = lstat(mappings[i].fsPath, &sb);

	if (rc) {
	    if (failedFile) *failedFile = mappings[i].fsPath;
	    return CPIOERR_STAT_FAILED;
	}

	if (!S_ISDIR(sb.st_mode) && sb.st_nlink > 1) {
	    hlink = hlinkList.next;
	    while (hlink && 
		   (hlink->dev != sb.st_dev || hlink->inode != sb.st_ino))
		hlink = hlink->next;
	    if (!hlink) {
		hlink = malloc(sizeof(*hlink));
		hlink->next = hlinkList.next;
		hlinkList.next = hlink;
		hlink->sb = sb;		/* structure assignment */
		hlink->dev = sb.st_dev;
		hlink->inode = sb.st_ino;
		hlink->nlink = sb.st_nlink;
		hlink->linksLeft = sb.st_nlink;
		hlink->fileMaps = malloc(sizeof(*hlink->fileMaps) * 
						sb.st_nlink);
	    }

	    hlink->fileMaps[--hlink->linksLeft] = i;

	    if (!hlink->linksLeft) {
		if ((rc = writeLinkedFile(cfd, hlink, mappings, cb, cbData,
			 		  &size, failedFile)))
		    return rc;

		totalsize += size;

		free(hlink->fileMaps);

		parent = &hlinkList;
		while (parent->next != hlink) parent = parent->next;
		parent->next = parent->next->next;
		free(hlink);
	    }
	} else {
	    if ((rc = writeFile(cfd, sb, mappings + i, &size, 1))) {
		if (failedFile) *failedFile = mappings[i].fsPath;
		return rc;
	    }

	    if (cb) {
		cbinfo.file = mappings[i].archivePath;
		cb(&cbinfo, cbData);
	    }

	    totalsize += size;
	}
    }    

    hlink = hlinkList.next;
    while (hlink) {
	if ((rc = writeLinkedFile(cfd, hlink, mappings, cb, cbData,
				  &size, failedFile)))
	    return rc;
	free(hlink->fileMaps);
	parent = hlink;
	hlink = hlink->next;
	free(parent);

	totalsize += size;
    }

    memset(&hdr, '0', PHYS_HDR_SIZE);
    memcpy(hdr.magic, CPIO_NEWC_MAGIC, sizeof(hdr.magic));
    memcpy(hdr.nlink, "00000001", 8);
    memcpy(hdr.namesize, "0000000b", 8);
    if ((rc = safewrite(cfd, &hdr, PHYS_HDR_SIZE)) != PHYS_HDR_SIZE)
	return rc;
    if ((rc = safewrite(cfd, "TRAILER!!!", 11)) != 11)
	return rc;
    totalsize += PHYS_HDR_SIZE + 11;

    /* GNU cpio pads to 512 bytes here, but we don't. I'm not sure if
       it matters or not */
    
    if ((rc = padoutfd(cfd, &totalsize, 4)))
	return rc;

    if (archiveSize) *archiveSize = totalsize;

    return 0;
}

const char * cpioStrerror(int rc)
{
    static char msg[256];
    char *s;
    int l, myerrno = errno;

    strcpy(msg, "cpio: ");
    switch (rc) {
    default:
	s = msg + strlen(msg);
	sprintf(s, _("(error 0x%x)"), rc);
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
    case CPIOERR_SYMLINK_FAILED: s = "symlink";	break;
    case CPIOERR_STAT_FAILED:	s = "stat";	break;
    case CPIOERR_MKDIR_FAILED:	s = "mkdir";	break;
    case CPIOERR_MKNOD_FAILED:	s = "mknod";	break;
    case CPIOERR_MKFIFO_FAILED:	s = "mkfifo";	break;
    case CPIOERR_LINK_FAILED:	s = "link";	break;
    case CPIOERR_READLINK_FAILED: s = "readlink";	break;
    case CPIOERR_READ_FAILED:	s = "read";	break;
    case CPIOERR_COPY_FAILED:	s = "copy";	break;

    case CPIOERR_INTERNAL:	s = _("Internal error");	break;
    case CPIOERR_HDR_SIZE:	s = _("Header size too big");	break;
    case CPIOERR_UNKNOWN_FILETYPE: s = _("Unknown file type");	break;
    }

    l = sizeof(msg) - strlen(msg) - 1;
    if (s != NULL) {
	if (l > 0) strncat(msg, s, l);
	l -= strlen(s);
    }
    if (rc & CPIOERR_CHECK_ERRNO) {
	s = _(" failed - ");
	if (l > 0) strncat(msg, s, l);
	l -= strlen(s);
	if (l > 0) strncat(msg, strerror(myerrno), l);
    }
    return msg;
}
