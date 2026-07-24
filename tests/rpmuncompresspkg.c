#include "system.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rpm/header.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmts.h>

#define RPM_LEAD_SIZE 96

struct entryInfo {
    uint32_t tag;
    uint32_t type;
    int32_t offset;
    uint32_t count;
};

static int rewriteHeaderEntry(const char *path, int mainHeader,
			      const rpmTagVal *tags, rpmTagVal newtag,
			      uint32_t newcount)
{
    uint32_t intro[4];
    struct entryInfo entry;
    off_t start = RPM_LEAD_SIZE;
    int fd = open(path, O_RDWR);
    int failed = 1;

    if (fd < 0 || pread(fd, intro, sizeof(intro), start) != sizeof(intro))
	goto exit;

    uint32_t count = ntohl(intro[2]);
    if (count > 1024 * 1024)
	goto exit;
    if (mainHeader) {
	uint32_t bytes = ntohl(intro[3]);
	off_t body = count * sizeof(entry) + bytes;
	start += sizeof(intro) + body + (8 - body % 8) % 8;
	if (pread(fd, intro, sizeof(intro), start) != sizeof(intro))
	    goto exit;
	count = ntohl(intro[2]);
	if (count > 1024 * 1024)
	    goto exit;
    }

    off_t entries = start + sizeof(intro);
    for (uint32_t i = 0; i < count; i++) {
	off_t pos = entries + i * sizeof(entry);
	if (pread(fd, &entry, sizeof(entry), pos) != sizeof(entry))
	    goto exit;
	for (int j = 0; tags[j]; j++) {
	    if (ntohl(entry.tag) == tags[j]) {
		if (newtag != RPMTAG_NOT_FOUND)
		    entry.tag = htonl(newtag);
		if (newcount)
		    entry.count = htonl(newcount);
		if (pwrite(fd, &entry, sizeof(entry), pos) == sizeof(entry))
		    failed = 0;
		goto exit;
	    }
	}
    }

exit:
    if (fd >= 0)
	close(fd);
    return failed;
}

static int setAltSize(rpmts ts, const char *path)
{
    FD_t fd = NULL;
    Header h = NULL;
    rpmRC rc;
    off_t payload;
    unsigned int hsize;
    uint64_t size = 1;
    int failed = 1;

    fd = Fopen(path, "r+.ufdio");
    if (fd == NULL || Ferror(fd))
	goto exit;
    rc = rpmReadPackageFile(ts, fd, NULL, &h);
    if (rc != RPMRC_OK && rc != RPMRC_NOKEY && rc != RPMRC_NOTTRUSTED)
	goto exit;
    payload = Ftell(fd);
    hsize = headerSizeof(h, 1);
    if (payload < (off_t)hsize ||
	!headerPutUint64(h, RPMTAG_PAYLOADSIZEALT, &size, 1) ||
	headerSizeof(h, 1) != hsize ||
	Fseek(fd, payload - hsize, SEEK_SET) < 0 ||
	headerWrite(fd, h, 1) || Fflush(fd))
	goto exit;
    failed = 0;

exit:
    headerFree(h);
    if (fd)
	Fclose(fd);
    return failed;
}

int main(int argc, char **argv)
{
    enum {
	MODE_NONE,
	MODE_NOHEADER,
	MODE_SMALLALTSIZE,
	MODE_UNKNOWNSIG,
	MODE_LEGACYONLY,
	MODE_TRANSFORMED,
	MODE_MULTIALT,
	MODE_MISSINGALT,
    } mode = MODE_NONE;
    static const rpmTagVal sizeTags[] = {
	RPMSIGTAG_SIZE, RPMSIGTAG_LONGSIZE, 0
    };
    static const rpmTagVal headerSignatures[] = {
	RPMSIGTAG_OPENPGP, RPMSIGTAG_RSA, RPMSIGTAG_DSA, 0
    };
    static const rpmTagVal legacySignatures[] = {
	RPMSIGTAG_PGP, RPMSIGTAG_GPG, RPMSIGTAG_PGP5, 0
    };
    static const rpmTagVal sha256Alt[] = {
	RPMTAG_PAYLOADSHA256ALT, 0
    };
    static const rpmTagVal sha512Alt[] = {
	RPMTAG_PAYLOADSHA512ALT, 0
    };
    rpmRC rc = RPMRC_FAIL;
    rpmts ts = NULL;
    FD_t fdi = NULL;
    FD_t fdo = NULL;
    int inplace;
    if (argc != 3 && argc != 4)
	return 2;
    if (argc == 4) {
	if (strcmp(argv[3], "noheader") == 0)
	    mode = MODE_NOHEADER;
	else if (strcmp(argv[3], "smallaltsize") == 0)
	    mode = MODE_SMALLALTSIZE;
	else if (strcmp(argv[3], "unknownsig") == 0)
	    mode = MODE_UNKNOWNSIG;
	else if (strcmp(argv[3], "legacyonly") == 0)
	    mode = MODE_LEGACYONLY;
	else if (strcmp(argv[3], "transformed") == 0)
	    mode = MODE_TRANSFORMED;
	else if (strcmp(argv[3], "multialt") == 0)
	    mode = MODE_MULTIALT;
	else if (strcmp(argv[3], "missingalt") == 0)
	    mode = MODE_MISSINGALT;
	else
	    return 2;
    }
    if (rpmReadConfigFiles(NULL, NULL)) {
	fprintf(stderr, "cannot read rpm configuration\n");
	return 2;
    }
    ts = rpmtsCreate();
    if (ts == NULL) {
	fprintf(stderr, "cannot create transaction set\n");
	goto exit;
    }
    if (rpmtsSetRootDir(ts, "/")) {
	fprintf(stderr, "cannot initialize transaction root\n");
	goto exit;
    }
    if (mode == MODE_NOHEADER || mode == MODE_SMALLALTSIZE ||
	mode == MODE_MULTIALT || mode == MODE_MISSINGALT)
	rpmtsSetVSFlags(ts, rpmtsVSFlags(ts) | RPMVSF_MASK_NOHEADER);
    if (mode == MODE_SMALLALTSIZE && setAltSize(ts, argv[1]))
	goto exit;
    if (mode == MODE_UNKNOWNSIG &&
	rewriteHeaderEntry(argv[1], 0, sizeTags, RPMTAG_SIG_BASE + 16, 0))
	goto exit;
    if (mode == MODE_LEGACYONLY) {
	if (rewriteHeaderEntry(argv[1], 0, headerSignatures,
			       RPMSIGTAG_LEMD5_1, 0) ||
	    rewriteHeaderEntry(argv[1], 0, legacySignatures,
			       RPMSIGTAG_PGP5, 0))
	    goto exit;
    }
    if (mode == MODE_MULTIALT &&
	rewriteHeaderEntry(argv[1], 1, sha256Alt, RPMTAG_NOT_FOUND, 2))
	goto exit;
    if (mode == MODE_MISSINGALT &&
	rewriteHeaderEntry(argv[1], 1, sha512Alt,
			   RPMTAG_PAYLOADSHA256ALT, 0))
	goto exit;
    inplace = (strcmp(argv[1], argv[2]) == 0);
    fdi = Fopen(argv[1], "r.ufdio");
    fdo = Fopen(argv[2], mode == MODE_TRANSFORMED ? "w.gzdio" :
			(inplace ? "r+.ufdio" : "w+.ufdio"));
    if (fdi == NULL || Ferror(fdi)) {
	fprintf(stderr, "cannot open input: %s\n", Fstrerror(fdi));
	goto exit;
    }
    if (fdo == NULL || Ferror(fdo)) {
	fprintf(stderr, "cannot open output: %s\n", Fstrerror(fdo));
	goto exit;
    }
    rc = rpmUncompressPackage(ts, fdi, fdo);

exit:
    if (fdo)
	Fclose(fdo);
    if (fdi)
	Fclose(fdi);
    rpmtsFree(ts);
    rpmFreeRpmrc();
    return rc == RPMRC_OK ? 0 : 1;
}
