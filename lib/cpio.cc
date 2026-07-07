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

#include "rpmio_internal.hh"	/* fdIsPlain, fdWriteZeros */
#include "rpmalign.hh"		/* rpmAlignIsValid */
#include "cpio.hh"

#include "debug.h"


struct rpmcpio_s {
    FD_t fd;
    char mode;
    off_t offset;
    off_t fileend;
    off_t base;		/*!< absolute backing-file offset of the cpio stream */
    size_t align;	/*!< content alignment, 0 if disabled */
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
    /* Plain streams use physical file offsets. Compressed streams align the
     * canonical cpio relative to its decompressed start. */
    if (fdIsPlain(fd)) {
	off_t base = Ftell(fd);
	cpio->base = (base > 0) ? base : 0;
    }
    return cpio;
}

off_t rpmcpioTell(rpmcpio_t cpio)
{
    return cpio->offset;
}

void rpmcpioSetWriteAlign(rpmcpio_t cpio, size_t align)
{
    cpio->align = rpmAlignIsValid(align) ? align : 0;
}

void rpmcpioSetReadAlign(rpmcpio_t cpio, size_t align)
{
    cpio->align = rpmAlignIsValid(align) ? align : 0;
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


/* Zero-fill the write stream up to the next @a modulo boundary. */
static off_t rpmcpioPadSize(rpmcpio_t cpio, size_t modulo, off_t base = 0)
{
    uint64_t rem = ((uint64_t)cpio->offset % modulo +
		    (uint64_t)base % modulo) % modulo;
    return (off_t)((modulo - rem) % modulo);
}

static int rpmcpioWritePad(rpmcpio_t cpio, size_t modulo, off_t base = 0)
{
    off_t left = rpmcpioPadSize(cpio, modulo, base);

    if (fdWriteZeros(cpio->fd, left))
	return RPMERR_WRITE_FAILED;
    cpio->offset += left;
    return 0;
}

/* Skip the read stream up to the next @a modulo boundary. */
static int rpmcpioReadPad(rpmcpio_t cpio, size_t modulo, off_t base = 0,
			  int require_zero = 0)
{
    off_t left = rpmcpioPadSize(cpio, modulo, base);
    char buf[BUFSIZ];

    while (left > 0) {
	size_t n = left > (off_t)sizeof(buf) ? sizeof(buf) : (size_t)left;
	if (Fread(buf, n, 1, cpio->fd) != n)
	    return RPMERR_READ_FAILED;
	if (require_zero) {
	    for (size_t i = 0; i < n; i++) {
		if (buf[i] != '\0')
		    return RPMERR_BAD_HEADER;
	    }
	}
	cpio->offset += n;
	left -= n;
    }
    return 0;
}

/* Content padding modulo. Regular files at least as large as the configured
 * alignment are padded to it; everything else keeps 4-byte cpio padding.
 * Non-regular content (a symlink target) is read inline before its size is
 * known, so aligning it would leave padding the reader cannot skip and desync
 * the stream. */
static int rpmcpioContentAligned(rpmcpio_t cpio, mode_t fmode, off_t fsize)
{
    return S_ISREG(fmode) && cpio->align >= 4 &&
	   fsize >= (off_t)cpio->align;
}

static int rpmcpioWriteContentPad(rpmcpio_t cpio, mode_t fmode, off_t fsize)
{
    int aligned = rpmcpioContentAligned(cpio, fmode, fsize);
    size_t modulo = aligned ? cpio->align : 4;
    off_t base = aligned ? cpio->base : 0;
    return rpmcpioWritePad(cpio, modulo, base);
}

static int rpmcpioReadContentPad(rpmcpio_t cpio, mode_t fmode, off_t fsize)
{
    int aligned = rpmcpioContentAligned(cpio, fmode, fsize);
    size_t modulo = aligned ? cpio->align : 4;
    off_t base = aligned ? cpio->base : 0;
    return rpmcpioReadPad(cpio, modulo, base, aligned);
}

#define GET_NUM_FIELD(phys, log) \
	\
	log = strntoul(phys, &end, 16, sizeof(phys)); \
	\
	if ( (end - phys) != sizeof(phys) ) return RPMERR_BAD_HEADER;
#define SET_NUM_FIELD(phys, val, space) \
	sprintf(space, "%8.8lx", (unsigned long) (val)); \
	\
	memcpy(phys, space, 8) \

static int rpmcpioTrailerWrite(rpmcpio_t cpio)
{
    struct cpioCrcPhysicalHeader hdr;
    int rc;
    size_t written;

    if (cpio->fileend != cpio->offset) {
        return RPMERR_WRITE_FAILED;
    }

    rc = rpmcpioWritePad(cpio, 4);
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

    rc = rpmcpioWritePad(cpio, 4);

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

    if (cpio->fileend != cpio->offset) {
        return RPMERR_WRITE_FAILED;
    }

    if (st->st_size >= CPIO_FILESIZE_MAX) {
	return RPMERR_FILE_SIZE;
    }

    rc = rpmcpioWritePad(cpio, 4);
    if (rc) {
        return rc;
    }

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

    /* Align the content when it is a regular file large enough to benefit. */
    rc = rpmcpioWriteContentPad(cpio, st->st_mode, st->st_size);

    cpio->fileend = cpio->offset + st->st_size;

    return rc;
}

int rpmcpioStrippedHeaderWrite(rpmcpio_t cpio, int fx, mode_t fmode,
			       off_t fsize)
{
    struct cpioStrippedPhysicalHeader hdr_s;
    struct cpioStrippedPhysicalHeader * hdr = &hdr_s;
    char field[64];
    size_t written;
    int rc = 0;

    if ((cpio->mode & O_ACCMODE) != O_WRONLY) {
        return RPMERR_WRITE_FAILED;
    }

    if (cpio->fileend != cpio->offset) {
        return RPMERR_WRITE_FAILED;
    }

    rc = rpmcpioWritePad(cpio, 4);
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

    /* Align the content when it is a regular file large enough to benefit. */
    rc = rpmcpioWriteContentPad(cpio, fmode, fsize);

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
    unsigned long fmode = 0;

    if ((cpio->mode & O_ACCMODE) != O_RDONLY) {
        return RPMERR_READ_FAILED;
    }

    /* Move to next file */
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

    rc = rpmcpioReadPad(cpio, 4);
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
        rc = rpmcpioReadPad(cpio, 4);

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
    GET_NUM_FIELD(hdr.mode, fmode);
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

    rc = rpmcpioReadContentPad(cpio, fmode, fsize);
    cpio->fileend = cpio->offset + fsize;

    if (!rc && rstreq(name, CPIO_TRAILER))
	rc = RPMERR_ITER_END;

    if (!rc && path)
	*path = xstrdup(name);

    return rc;
}

int rpmcpioSetExpectedFileSize(rpmcpio_t cpio, mode_t fmode, off_t fsize)
{
    int rc = 0;
    /* Stripped headers carry no size, so the content alignment padding could not
     * be skipped at header-read time; skip it now that the size is known. */
    if (rpmcpioContentAligned(cpio, fmode, fsize))
	rc = rpmcpioReadContentPad(cpio, fmode, fsize);
    cpio->fileend = cpio->offset + fsize;
    return rc;
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
