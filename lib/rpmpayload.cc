#include "system.h"

#include <cstring>
#include <errno.h>

#include "rpmalign.hh"
#include "rpmpayload.hh"

static int probeError(FD_t fd, off_t start, int error)
{
    int save = error ? error : EIO;
    (void) Fseek(fd, start, SEEK_SET);
    errno = save;
    return -1;
}

int rpmPayloadProbe(FD_t fd, Header h, struct rpmPayloadInfo *info)
{
    const char *compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);
    uint64_t alignment = headerGetNumber(h, RPMTAG_PAYLOADALIGNMENT);
    char magic[4];
    char buf[BUFSIZ];
    off_t start = Ftell(fd);
    off_t aligned;

    if (info == NULL) {
	errno = EINVAL;
	return -1;
    }
    if (start < 0)
	return -1;
    *info = {};

    errno = 0;
    if (Fread(magic, 1, sizeof(magic), fd) != (ssize_t)sizeof(magic))
	return probeError(fd, start, errno);
    if (Fseek(fd, start, SEEK_SET) < 0)
	return -1;

    info->start = start;
    if (memcmp(magic, "0707", sizeof(magic)) == 0) {
	info->io = "ufdio";
	info->raw = 1;
	return 0;
    }

    /* Compression formats have non-NUL magic. A missing compressor tag on
     * legacy packages means gzip, not an uncompressed payload. */
    if (magic[0] != '\0') {
	info->io = (compr && compr[0]) ? compr : "gzip";
	return 0;
    }

    if (alignment > SIZE_MAX || !rpmAlignIsValid((size_t)alignment))
	return probeError(fd, start, EINVAL);
    aligned = rpmAlignUp(start, (size_t)alignment);
    if (aligned < start)
	return probeError(fd, start, EINVAL);

    for (off_t pos = start; pos < aligned; ) {
	size_t n = (aligned - pos > (off_t)sizeof(buf)) ?
		   sizeof(buf) : (size_t)(aligned - pos);
	errno = 0;
	if (Fread(buf, 1, n, fd) != (ssize_t)n)
	    return probeError(fd, start, errno);
	for (size_t i = 0; i < n; i++) {
	    if (buf[i] != '\0')
		return probeError(fd, start, EINVAL);
	}
	pos += n;
    }

    errno = 0;
    if (Fread(magic, 1, sizeof(magic), fd) != (ssize_t)sizeof(magic))
	return probeError(fd, start, errno);
    if (memcmp(magic, "0707", sizeof(magic)) != 0)
	return probeError(fd, start, EINVAL);
    if (Fseek(fd, aligned, SEEK_SET) < 0)
	return probeError(fd, start, errno);

    info->start = aligned;
    info->io = "ufdio";
    info->raw = 1;
    return 0;
}
