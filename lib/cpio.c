/** \ingroup payload
 * \file lib/cpio.c
 *  Handle cpio payloads within rpm packages.
 *
 * \warning FIXME: We don't translate between cpio and system mode bits! These
 * should both be the same, but really odd things are going to happen if
 * that's not true!
 */

#include "system.h"

#if MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#else
#include <sys/types.h> /* already included from system.h */
#endif
#include <errno.h>
#include <string.h>

#include <rpm/rpmio.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "lib/cpio.h"

#include "debug.h"


struct rpmcpio_s {
    FD_t fd;
    char mode;
    off_t offset;
    off_t fileend;
};

rpmcpio_t rpmcpioOpen(FD_t fd, char mode)
{
    if ((mode & O_ACCMODE) != O_RDONLY &&
        (mode & O_ACCMODE) != O_WRONLY)
        return NULL;

    rpmcpio_t cpio = xcalloc(1, sizeof(*cpio));
    cpio->fd = fdLink(fd);
    cpio->mode = mode;
    cpio->offset = 0;
    return cpio;
}

off_t rpmcpioTell(rpmcpio_t cpio)
{
    return cpio->offset;
}


/**
 * Convert string to unsigned integer (with buffer size check).
 * @param str		input string
 * @retval endptr	address of 1st character not processed
 * @param base		numerical conversion base
 * @param num		max no. of bytes to read
 * @return		converted integer
 */
static unsigned long strntoul(const char *str,char **endptr, int base, size_t num)
{
    char buf[num+1], * end;
    unsigned long ret;

    strncpy(buf, str, num);
    buf[num] = '\0';

    ret = strtoul(buf, &end, base);
    if (*end != '\0')
	*endptr = ((char *)str) + (end - buf);	/* XXX discards const */
    else
	*endptr = ((char *)str) + strlen(buf);

    return ret;
}


static int rpmcpioWritePad(rpmcpio_t cpio, ssize_t modulo)
{
    char buf[modulo];
    ssize_t left, writen;
    memset(buf, 0, modulo);
    left = (modulo - ((cpio->offset) % modulo)) % modulo;
    if (left <= 0)
        return 0;
    writen = Fwrite(&buf, left, 1, cpio->fd);
    if (writen != left) {
        return CPIOERR_WRITE_FAILED;
    }
    cpio->offset += writen;
    return 0;
}

static int rpmcpioReadPad(rpmcpio_t cpio)
{
    ssize_t modulo = 4;
    char buf[4];
    ssize_t left, read;
    left = (modulo - (cpio->offset % modulo)) % modulo;
    if (left <= 0)
        return 0;
    read = Fread(&buf, left, 1, cpio->fd);
    cpio->offset += read;
    if (read != left) {
        return CPIOERR_READ_FAILED;
    }
    return 0;
}

#define GET_NUM_FIELD(phys, log) \
	\
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	\
	if ( (end - phys) != sizeof(phys) ) return CPIOERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	\
	memcpy(phys, space, 8) \

static int rpmcpioTrailerWrite(rpmcpio_t cpio)
{
    struct cpioCrcPhysicalHeader hdr;
    int rc;
    size_t writen;

    if (cpio->fileend != cpio->offset) {
        return CPIOERR_WRITE_FAILED;
    }

    rc = rpmcpioWritePad(cpio, 4);
    if (rc)
        return rc;

    memset(&hdr, '0', PHYS_HDR_SIZE);
    memcpy(&hdr.magic, CPIO_NEWC_MAGIC, sizeof(hdr.magic));
    memcpy(&hdr.nlink, "00000001", 8);
    memcpy(&hdr.namesize, "0000000b", 8);
    writen = Fwrite(&hdr, PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += writen;
    if (writen != PHYS_HDR_SIZE) {
        return CPIOERR_WRITE_FAILED;
    }
    writen = Fwrite(&CPIO_TRAILER, sizeof(CPIO_TRAILER), 1, cpio->fd);
    cpio->offset += writen;
    if (writen != sizeof(CPIO_TRAILER)) {
        return CPIOERR_WRITE_FAILED;
    }

    /*
     * XXX GNU cpio pads to 512 bytes. This may matter for
     * tape device(s) and/or concatenated cpio archives.
     */

    rc = rpmcpioWritePad(cpio, 4);

    return rc;
}

int rpmcpioHeaderWrite(rpmcpio_t cpio, char * path, struct stat * st)
{
    struct cpioCrcPhysicalHeader hdr_s;
    struct cpioCrcPhysicalHeader * hdr = &hdr_s;
    char field[64];
    size_t len, writen;
    dev_t dev;
    int rc = 0;

    if ((cpio->mode & O_ACCMODE) != O_WRONLY) {
        return CPIOERR_WRITE_FAILED;
    }

    if (cpio->fileend != cpio->offset) {
        return CPIOERR_WRITE_FAILED;
    }

    if (st->st_size >= CPIO_FILESIZE_MAX) {
	return CPIOERR_FILE_SIZE;
    }

    rc = rpmcpioWritePad(cpio, 4);
    if (rc) {
        return rc;
    }

    memcpy(hdr->magic, CPIO_NEWC_MAGIC, sizeof(hdr->magic));
    SET_NUM_FIELD(hdr->inode, st->st_ino, field);
    SET_NUM_FIELD(hdr->mode, st->st_mode, field);
    SET_NUM_FIELD(hdr->uid, st->st_uid, field);
    SET_NUM_FIELD(hdr->gid, st->st_gid, field);
    SET_NUM_FIELD(hdr->nlink, st->st_nlink, field);
    SET_NUM_FIELD(hdr->mtime, st->st_mtime, field);
    SET_NUM_FIELD(hdr->filesize, st->st_size, field);

    dev = major(st->st_dev); SET_NUM_FIELD(hdr->devMajor, dev, field);
    dev = minor(st->st_dev); SET_NUM_FIELD(hdr->devMinor, dev, field);
    dev = major(st->st_rdev); SET_NUM_FIELD(hdr->rdevMajor, dev, field);
    dev = minor(st->st_rdev); SET_NUM_FIELD(hdr->rdevMinor, dev, field);

    len = strlen(path) + 1;
    SET_NUM_FIELD(hdr->namesize, len, field);

    memcpy(hdr->checksum, "00000000", 8);

    writen = Fwrite(hdr, PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += writen;
    if (writen != PHYS_HDR_SIZE) {
        return CPIOERR_WRITE_FAILED;
    }

    writen = Fwrite(path, len, 1, cpio->fd);
    cpio->offset += writen;
    if (writen != len) {
        return CPIOERR_WRITE_FAILED;
    }

    rc = rpmcpioWritePad(cpio, 4);

    cpio->fileend = cpio->offset + st->st_size;

    return rc;
}

ssize_t rpmcpioWrite(rpmcpio_t cpio, void * buf, size_t size)
{
    size_t writen, left;

    if ((cpio->mode & O_ACCMODE) != O_WRONLY) {
        return CPIOERR_WRITE_FAILED;
    }

    // Do not write beyond file length
    left = cpio->fileend - cpio->offset;
    size = size > left ? left : size;
    writen = Fwrite(buf, size, 1, cpio->fd);
    cpio->offset += writen;
    return writen;
}


int rpmcpioHeaderRead(rpmcpio_t cpio, char ** path, struct stat * st)
{
    struct cpioCrcPhysicalHeader hdr;
    int nameSize;
    char * end;
    unsigned int major, minor;
    int rc = 0;
    ssize_t read;

    if ((cpio->mode & O_ACCMODE) != O_RDONLY) {
        return CPIOERR_READ_FAILED;
    }

    /* Move to next file */
    if (cpio->fileend != cpio->offset) {
        /* XXX try using Fseek() - which is currently broken */
        char buf[8*BUFSIZ];
        while (cpio->fileend != cpio->offset) {
            read = cpio->fileend - cpio->offset > 8*BUFSIZ ? 8*BUFSIZ : cpio->fileend - cpio->offset;
            if (rpmcpioRead(cpio, &buf, read) != read) {
                return CPIOERR_READ_FAILED;
            }
        }
    }

    rc = rpmcpioReadPad(cpio);
    if (rc) return rc;

    read = Fread(&hdr, PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += read;
    if (read != PHYS_HDR_SIZE)
	return CPIOERR_READ_FAILED;

    if (strncmp(CPIO_CRC_MAGIC, hdr.magic, sizeof(CPIO_CRC_MAGIC)-1) &&
	strncmp(CPIO_NEWC_MAGIC, hdr.magic, sizeof(CPIO_NEWC_MAGIC)-1)) {
	return CPIOERR_BAD_MAGIC;
    }
    GET_NUM_FIELD(hdr.inode, st->st_ino);
    GET_NUM_FIELD(hdr.mode, st->st_mode);
    GET_NUM_FIELD(hdr.uid, st->st_uid);
    GET_NUM_FIELD(hdr.gid, st->st_gid);
    GET_NUM_FIELD(hdr.nlink, st->st_nlink);
    GET_NUM_FIELD(hdr.mtime, st->st_mtime);
    GET_NUM_FIELD(hdr.filesize, st->st_size);

    GET_NUM_FIELD(hdr.devMajor, major);
    GET_NUM_FIELD(hdr.devMinor, minor);
    st->st_dev = makedev(major, minor);

    GET_NUM_FIELD(hdr.rdevMajor, major);
    GET_NUM_FIELD(hdr.rdevMinor, minor);
    st->st_rdev = makedev(major, minor);

    GET_NUM_FIELD(hdr.namesize, nameSize);

    *path = xmalloc(nameSize + 1);
    read = Fread(*path, nameSize, 1, cpio->fd);
    (*path)[nameSize] = '\0';
    cpio->offset += read;
    if (read != nameSize ) {
        return CPIOERR_BAD_HEADER;
    }

    rc = rpmcpioReadPad(cpio);
    cpio->fileend = cpio->offset + st->st_size;

    if (!rc && rstreq(*path, CPIO_TRAILER))
	rc = CPIOERR_HDR_TRAILER;

    return rc;
}

ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size)
{
    size_t read, left;

    if ((cpio->mode & O_ACCMODE) != O_RDONLY) {
        return CPIOERR_READ_FAILED;
    }

    left = cpio->fileend - cpio->offset;
    size = size > left ? left : size;
    read = Fread(buf, size, 1, cpio->fd);
    cpio->offset += read;
    return read;
}

int rpmcpioClose(rpmcpio_t cpio)
{
    int rc = 0;
    if ((cpio->mode & O_ACCMODE) == O_WRONLY) {
        rc = rpmcpioTrailerWrite(cpio);
    }
    fdFree(cpio->fd);
    cpio->fd = NULL;
    return rc;
}

rpmcpio_t rpmcpioFree(rpmcpio_t cpio)
{
    if (cpio) {
	if (cpio->fd)
	    (void) rpmcpioClose(cpio);
	free(cpio);
    }
    return NULL;
}

const char * rpmcpioStrerror(int rc)
{
    static char msg[256];
    const char *s;
    int myerrno = errno;
    size_t l;

    strcpy(msg, "cpio: ");
    switch (rc) {
    default: {
	char *t = msg + strlen(msg);
	sprintf(t, _("(error 0x%x)"), (unsigned)rc);
	s = NULL;
	break;
    }
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
    case CPIOERR_LSETFCON_FAILED: s = "lsetfilecon";	break;
    case CPIOERR_SETCAP_FAILED: s = "cap_set_file";	break;

    case CPIOERR_HDR_SIZE:	s = _("Header size too big");	break;
    case CPIOERR_FILE_SIZE:	s = _("File too large for archive");	break;
    case CPIOERR_UNKNOWN_FILETYPE: s = _("Unknown file type");	break;
    case CPIOERR_MISSING_HARDLINK: s = _("Missing hard link(s)"); break;
    case CPIOERR_DIGEST_MISMATCH: s = _("Digest mismatch");	break;
    case CPIOERR_INTERNAL:	s = _("Internal error");	break;
    case CPIOERR_UNMAPPED_FILE:	s = _("Archive file not in header"); break;
    case CPIOERR_ENOENT:	s = strerror(ENOENT); break;
    case CPIOERR_ENOTEMPTY:	s = strerror(ENOTEMPTY); break;
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
