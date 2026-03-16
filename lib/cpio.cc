/** \ingroup payload
 * \file lib/cpio.c
 *  Handle cpio payloads within rpm packages.
 *
 * \warning FIXME: We don't translate between cpio and system mode bits! These
 * should both be the same, but really odd things are going to happen if
 * that's not true!
 */

#include "system.h"

#include <string>

#if defined(MAJOR_IN_MKDEV)
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS)
#include <sys/sysmacros.h>
#else
#include <sys/types.h> /* already included from system.h */
#endif
#include <string.h>
#include <fcntl.h>

#include <rpm/rpmio.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmarchive.h>

#include "cpio.hh"

#include "debug.h"


/**
 * CPIO archive state, used for both reading and writing.
 *
 * offset tracks the current byte position in the archive stream. fileend is
 * set to (offset + file_size) when a header is written/read, marking where
 * the current entry's data ends. Every header write/read function checks
 * (fileend == offset) to verify the previous entry was fully consumed before
 * proceeding — a mismatch is treated as an error during writes, and during
 * reads the remaining bytes are skipped.
 */
struct rpmcpio_s {
    FD_t fd;
    char mode;
    off_t offset;	/*!< current position in the archive stream */
    off_t fileend;	/*!< expected end position of current entry's data */
};

/*
 * Size limit for individual files in "new ascii format" cpio archives.
 * The max size of the entire archive is unlimited from cpio POV,
 * but subject to filesystem limitations.
 */
#define CPIO_FILESIZE_MAX UINT32_MAX
#define CPIO_NAMESIZE_MAX 4096

#define CPIO_NEWC_MAGIC	"070701"
#define CPIO_CRC_MAGIC	"070702"
#define CPIO_STRIPPED_MAGIC "07070X"
#define CPIO_TRAILER	"TRAILER!!!"

/** \ingroup payload
 * Cpio archive header information.
 */
struct cpioCrcPhysicalHeader {
    /* char magic[6]; handled separately */
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

#define	PHYS_HDR_SIZE	104		/* Don't depend on sizeof(struct) */

struct cpioStrippedPhysicalHeader {
    /* char magic[6]; handled separately */
    char fx[8];
};

#define STRIPPED_PHYS_HDR_SIZE 8       /* Don't depend on sizeof(struct) */

rpmcpio_t rpmcpioOpen(FD_t fd, char mode)
{
    if ((mode & O_ACCMODE) != O_RDONLY &&
        (mode & O_ACCMODE) != O_WRONLY)
        return NULL;

    rpmcpio_t cpio = new rpmcpio_s {};
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
 * @param[out] endptr	address of 1st character not processed
 * @param base		numerical conversion base
 * @param num		max no. of bytes to read
 * @return		converted integer
 */
static unsigned long strntoul(const char *str,char **endptr, int base, size_t num)
{
    std::string buf(str,num);
    char *end;

    unsigned long ret = strtoul(buf.c_str(), &end, base);

    if (*end != '\0')
	*endptr = ((char *)str) + (end - buf.c_str());	/* XXX discards const */
    else
	*endptr = ((char *)str) + buf.size();

    return ret;
}


/**
 * Write NUL padding bytes to align cpio->offset to a 4-byte boundary.
 *
 * In newc CPIO format, both header+name and file data regions are independently
 * padded to 4-byte boundaries. This means:
 *   - After writing magic + header + name, pad so file data starts at a 4-byte boundary.
 *   - After writing file data, pad so the next header starts at a 4-byte boundary.
 *
 * This function is called from multiple places:
 *   - rpmcpioHeaderWrite: once after the header+name, and the previous entry's
 *     file data is implicitly padded by calling this at the *start* of the function
 *     (before the new header is written).
 *   - rpmcpioStrippedHeaderWrite: same pattern — pads the previous entry's data
 *     at the start, then pads the stripped header itself (which is 14 bytes, needing
 *     2 bytes of padding to reach 16).
 *   - rpmcpioTrailerWrite: pads the last file's data before writing the trailer.
 *
 * Because padding is based on the global offset rather than individual section
 * lengths, the padding for one section's data and the next section's header are
 * combined into a single pad call. This works because each section starts at an
 * already-aligned offset, so the data padding amount depends only on the data size.
 */
static int rpmcpioWritePad(rpmcpio_t cpio)
{
    const ssize_t modulo = 4;
    char buf[modulo];
    ssize_t left, written;
    memset(buf, 0, modulo);
    left = (modulo - ((cpio->offset) % modulo)) % modulo;
    if (left <= 0)
        return 0;
    written = Fwrite(&buf, left, 1, cpio->fd);
    if (written != left) {
        return RPMERR_WRITE_FAILED;
    }
    cpio->offset += written;
    return 0;
}

static int rpmcpioReadPad(rpmcpio_t cpio)
{
    const ssize_t modulo = 4;
    char buf[modulo];
    ssize_t left, read;
    left = (modulo - (cpio->offset % modulo)) % modulo;
    if (left <= 0)
        return 0;
    read = Fread(&buf, left, 1, cpio->fd);
    cpio->offset += read;
    if (read != left) {
        return RPMERR_READ_FAILED;
    }
    return 0;
}

#define GET_NUM_FIELD(phys, log) \
	\
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	\
	if ( (end - phys) != sizeof(phys) ) return RPMERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	\
	memcpy(phys, space, 8)

/**
 * Write the CPIO archive trailer.
 *
 * The trailer is a standard newc header with the special name "TRAILER!!!".
 * All fields are zero except nlink (1) and namesize (11 = strlen("TRAILER!!!") + 1).
 * The trailer itself is also padded to a 4-byte boundary.
 *
 * This is called automatically from rpmcpioClose() for write-mode archives.
 * A standard newc trailer is used for both regular and stripped CPIO archives.
 */
static int rpmcpioTrailerWrite(rpmcpio_t cpio)
{
    struct cpioCrcPhysicalHeader hdr;
    int rc;
    size_t written;

    /* Verify the last entry's data was fully written */
    if (cpio->fileend != cpio->offset) {
        return RPMERR_WRITE_FAILED;
    }

    /* Pad the last entry's file data to a 4-byte boundary */
    rc = rpmcpioWritePad(cpio);
    if (rc)
        return rc;

    memset(&hdr, '0', PHYS_HDR_SIZE);
    memcpy(&hdr.nlink, "00000001", 8);
    memcpy(&hdr.namesize, "0000000b", 8);

    written = Fwrite(CPIO_NEWC_MAGIC, 6, 1, cpio->fd);
    cpio->offset += written;
    if (written != 6) {
        return RPMERR_WRITE_FAILED;
    }

    written = Fwrite(&hdr, PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += written;
    if (written != PHYS_HDR_SIZE) {
        return RPMERR_WRITE_FAILED;
    }
    written = Fwrite(&CPIO_TRAILER, sizeof(CPIO_TRAILER), 1, cpio->fd);
    cpio->offset += written;
    if (written != sizeof(CPIO_TRAILER)) {
        return RPMERR_WRITE_FAILED;
    }

    /*
     * XXX GNU cpio pads to 512 bytes. This may matter for
     * tape device(s) and/or concatenated cpio archives.
     */

    rc = rpmcpioWritePad(cpio);

    return rc;
}

int rpmcpioHeaderWrite(rpmcpio_t cpio, char * path, struct stat * st)
{
    struct cpioCrcPhysicalHeader hdr_s;
    struct cpioCrcPhysicalHeader * hdr = &hdr_s;
    char field[64];
    size_t len, written;
    dev_t dev;
    int rc = 0;

    if ((cpio->mode & O_ACCMODE) != O_WRONLY) {
        return RPMERR_WRITE_FAILED;
    }

    /* Verify that the previous entry's data was fully written */
    if (cpio->fileend != cpio->offset) {
        return RPMERR_WRITE_FAILED;
    }

    if (st->st_size >= CPIO_FILESIZE_MAX) {
	return RPMERR_FILE_SIZE;
    }

    /* Pad the previous entry's file data to a 4-byte boundary */
    rc = rpmcpioWritePad(cpio);
    if (rc) {
        return rc;
    }

    /*
     * The filesize field in the CPIO header has different meanings depending on
     * the file type:
     *   - Regular files: the size of the file content that follows.
     *   - Symlinks: the length of the link target string (which is written as
     *     the "file data"). The link target is NOT NUL-terminated in the archive.
     *   - Directories: 0 (no data follows).
     *   - Ghost files: never reach this code path — they are excluded from the
     *     archive entirely (see rpmfiArchiveWriteFile in rpmfi.cc).
     */
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

    /* CPIO checksum not used */
    memcpy(hdr->checksum, "00000000", 8);

    written = Fwrite(CPIO_NEWC_MAGIC, 6, 1, cpio->fd);
    cpio->offset += written;
    if (written != 6) {
        return RPMERR_WRITE_FAILED;
    }

    written = Fwrite(hdr, PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += written;
    if (written != PHYS_HDR_SIZE) {
        return RPMERR_WRITE_FAILED;
    }

    written = Fwrite(path, len, 1, cpio->fd);
    cpio->offset += written;
    if (written != len) {
        return RPMERR_WRITE_FAILED;
    }

    rc = rpmcpioWritePad(cpio);

    cpio->fileend = cpio->offset + st->st_size;

    return rc;
}

int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, off_t fsize)
{
    struct cpioStrippedPhysicalHeader hdr_s;
    struct cpioStrippedPhysicalHeader * hdr = &hdr_s;
    char field[64];
    size_t written;
    int rc = 0;

    if ((cpio->mode & O_ACCMODE) != O_WRONLY) {
        return RPMERR_WRITE_FAILED;
    }

    /* Verify that the previous entry's data was fully written */
    if (cpio->fileend != cpio->offset) {
        return RPMERR_WRITE_FAILED;
    }

    /* Pad the previous entry's file data to a 4-byte boundary */
    rc = rpmcpioWritePad(cpio);
    if (rc) {
        return rc;
    }

    SET_NUM_FIELD(hdr->fx, fx, field);

    written = Fwrite(CPIO_STRIPPED_MAGIC, 6, 1, cpio->fd);
    cpio->offset += written;
    if (written != 6) {
        return RPMERR_WRITE_FAILED;
    }

    written = Fwrite(hdr, STRIPPED_PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += written;
    if (written != STRIPPED_PHYS_HDR_SIZE) {
        return RPMERR_WRITE_FAILED;
    }

    /* Pad the header itself to a 4-byte boundary (14 -> 16 bytes) */
    rc = rpmcpioWritePad(cpio);

    cpio->fileend = cpio->offset + fsize;

    return rc;
}

ssize_t rpmcpioWrite(rpmcpio_t cpio, const void * buf, size_t size)
{
    size_t written, left;

    if ((cpio->mode & O_ACCMODE) != O_WRONLY) {
        return RPMERR_WRITE_FAILED;
    }

    // Do not write beyond file length
    left = cpio->fileend - cpio->offset;
    size = size > left ? left : size;
    written = Fwrite(buf, size, 1, cpio->fd);
    cpio->offset += written;
    return written;
}

int rpmcpioHeaderRead(rpmcpio_t cpio, char ** path, int * fx)
{
    struct cpioCrcPhysicalHeader hdr;
    int nameSize;
    char * end;
    int rc = 0;
    ssize_t read;
    char magic[6];
    rpm_loff_t fsize;

    if ((cpio->mode & O_ACCMODE) != O_RDONLY) {
        return RPMERR_READ_FAILED;
    }

    /* Skip any unread data from the previous entry */
    if (cpio->fileend != cpio->offset) {
        /* XXX try using Fseek() - which is currently broken */
        char buf[8*BUFSIZ];
        while (cpio->fileend != cpio->offset) {
            read = cpio->fileend - cpio->offset > 8*BUFSIZ ? 8*BUFSIZ : cpio->fileend - cpio->offset;
            if (rpmcpioRead(cpio, &buf, read) != read) {
                return RPMERR_READ_FAILED;
            }
        }
    }

    /* Pad past the previous entry's file data to reach the next header */
    rc = rpmcpioReadPad(cpio);
    if (rc) return rc;

    read = Fread(&magic, 6, 1, cpio->fd);
    cpio->offset += read;
    if (read != 6)
	return RPMERR_BAD_MAGIC;

    /* read stripped header */
    if (!strncmp(CPIO_STRIPPED_MAGIC, magic,
                 sizeof(CPIO_STRIPPED_MAGIC)-1)) {
        struct cpioStrippedPhysicalHeader shdr;
        read = Fread(&shdr, STRIPPED_PHYS_HDR_SIZE, 1, cpio->fd);
        cpio->offset += read;
        if (read != STRIPPED_PHYS_HDR_SIZE)
	    return RPMERR_BAD_HEADER;

        GET_NUM_FIELD(shdr.fx, *fx);
        rc = rpmcpioReadPad(cpio);

        if (!rc && *fx == -1)
            rc = RPMERR_ITER_END;
        return rc;
    }

    if (strncmp(CPIO_CRC_MAGIC, magic, sizeof(CPIO_CRC_MAGIC)-1) &&
	strncmp(CPIO_NEWC_MAGIC, magic, sizeof(CPIO_NEWC_MAGIC)-1)) {
	return RPMERR_BAD_MAGIC;
    }

    read = Fread(&hdr, PHYS_HDR_SIZE, 1, cpio->fd);
    cpio->offset += read;
    if (read != PHYS_HDR_SIZE)
        return RPMERR_BAD_HEADER;

    GET_NUM_FIELD(hdr.filesize, fsize);
    GET_NUM_FIELD(hdr.namesize, nameSize);
    if (nameSize <= 0 || nameSize > CPIO_NAMESIZE_MAX) {
        return RPMERR_BAD_HEADER;
    }

    char name[CPIO_NAMESIZE_MAX + 1];
    read = Fread(name, nameSize, 1, cpio->fd);
    name[nameSize] = '\0';
    cpio->offset += read;
    if (read != nameSize ) {
        return RPMERR_BAD_HEADER;
    }

    rc = rpmcpioReadPad(cpio);
    cpio->fileend = cpio->offset + fsize;

    if (!rc && rstreq(name, CPIO_TRAILER))
	rc = RPMERR_ITER_END;

    if (!rc && path)
	*path = xstrdup(name);

    return rc;
}

void rpmcpioSetExpectedFileSize(rpmcpio_t cpio, off_t fsize)
{
    cpio->fileend = cpio->offset + fsize;
}

ssize_t rpmcpioRead(rpmcpio_t cpio, void * buf, size_t size)
{
    size_t read, left;

    if ((cpio->mode & O_ACCMODE) != O_RDONLY) {
        return RPMERR_READ_FAILED;
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
	delete cpio;
    }
    return NULL;
}
