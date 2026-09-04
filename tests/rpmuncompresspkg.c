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

/*
 * Overwrite a scalar INT64 value in the raw main header. The header returned
 * by rpmReadPackageFile() carries merged signature tags, so its serialized
 * size does not locate the stored header; patch the blob in place instead.
 */
static int setRawUint64(const char *path, rpmTagVal tag, uint64_t value)
{
    uint32_t intro[4];
    struct entryInfo entry;
    off_t start = RPM_LEAD_SIZE;
    int fd = open(path, O_RDWR);
    int failed = 1;

    if (fd < 0 || pread(fd, intro, sizeof(intro), start) != sizeof(intro))
	goto exit;

    uint32_t count = ntohl(intro[2]);
    uint32_t bytes = ntohl(intro[3]);
    if (count > 1024 * 1024)
	goto exit;

    /* Step over the signature header, including its 8-byte alignment. */
    off_t body = count * sizeof(entry) + bytes;
    start += sizeof(intro) + body + (8 - body % 8) % 8;
    if (pread(fd, intro, sizeof(intro), start) != sizeof(intro))
	goto exit;
    count = ntohl(intro[2]);
    if (count > 1024 * 1024)
	goto exit;

    off_t entries = start + sizeof(intro);
    off_t data = entries + (off_t)count * sizeof(entry);
    for (uint32_t i = 0; i < count; i++) {
	if (pread(fd, &entry, sizeof(entry), entries + i * sizeof(entry)) !=
	    sizeof(entry))
	    goto exit;
	if (ntohl(entry.tag) != tag)
	    continue;
	if (ntohl(entry.type) != RPM_INT64_TYPE || ntohl(entry.count) != 1)
	    goto exit;
	unsigned char be[8];
	for (int k = 0; k < 8; k++)
	    be[k] = (unsigned char)(value >> (56 - 8 * k));
	if (pwrite(fd, be, sizeof(be), data + (int32_t)ntohl(entry.offset)) ==
	    sizeof(be))
	    failed = 0;
	goto exit;
    }

exit:
    if (fd >= 0)
	close(fd);
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
	MODE_REUSE,
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
	else if (strcmp(argv[3], "reuse") == 0)
	    mode = MODE_REUSE;
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
    if (mode == MODE_SMALLALTSIZE &&
	setRawUint64(argv[1], RPMTAG_PAYLOADSIZEALT, 1))
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

    /*
     * Materializing twice into the same reused output must be deterministic;
     * a digest context left attached to the caller's fd would corrupt the
     * second pass.
     */
    if (rc == RPMRC_OK && mode == MODE_REUSE) {
	off_t len = Ftell(fdo);
	char *first = (len >= 0) ? malloc(len ? len : 1) : NULL;
	if (first == NULL || Fseek(fdo, 0, SEEK_SET) < 0 ||
	    Fread(first, 1, len, fdo) != len) {
	    free(first);
	    rc = RPMRC_FAIL;
	    goto exit;
	}
	rc = rpmUncompressPackage(ts, fdi, fdo);
	if (rc == RPMRC_OK) {
	    char *second = malloc(len ? len : 1);
	    if (second == NULL || Ftell(fdo) != len ||
		Fseek(fdo, 0, SEEK_SET) < 0 ||
		Fread(second, 1, len, fdo) != len ||
		memcmp(first, second, len)) {
		fprintf(stderr, "reused output differs between passes\n");
		rc = RPMRC_FAIL;
	    }
	    free(second);
	}
	free(first);
    }

exit:
    if (fdo)
	Fclose(fdo);
    if (fdi)
	Fclose(fdi);
    rpmtsFree(ts);
    rpmFreeRpmrc();
    return rc == RPMRC_OK ? 0 : 1;
}
