#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <rpm/rpmtag.h>

struct entryInfo {
    uint32_t tag;
    uint32_t type;
    int32_t offset;
    uint32_t count;
};

static const char * const tagTypeNames[] = {
    "(null)", "char", "int8", "int16", "int32", "int64",
    "string", "blob", "argv", "i18nstring"
};

static const char *sigTagName(uint32_t tag)
{
    const char *n = "(unknown)";
    switch (tag) {
    case RPMTAG_HEADERSIGNATURES: return "Headersignatures";
    case RPMTAG_HEADERIMAGE: return "Headerimage";
    case RPMSIGTAG_SIZE: return "Size";
    case RPMSIGTAG_LEMD5_1: return "Lemd5_1";
    case RPMSIGTAG_PGP: return "Pgp";
    case RPMSIGTAG_LEMD5_2: return "Lemd5_2";
    case RPMSIGTAG_MD5: return "Md5";
    case RPMSIGTAG_GPG: return "Gpg";
    case RPMSIGTAG_PGP5: return "Pgp5";
    case RPMSIGTAG_PAYLOADSIZE: return "Payloadsize";
    case RPMSIGTAG_RESERVEDSPACE: return "Reservedspace";
    case RPMSIGTAG_BADSHA1_1: return "Badsha1_1";
    case RPMSIGTAG_BADSHA1_2: return "Badsha1_2";
    case RPMSIGTAG_SHA1: return "Sha1";
    case RPMSIGTAG_DSA: return "Dsa";
    case RPMSIGTAG_RSA: return "Rsa";
    case RPMSIGTAG_LONGSIZE: return "Longsize";
    case RPMSIGTAG_LONGARCHIVESIZE: return "Longarchivesize";
    case RPMSIGTAG_SHA256: return "Sha256";
    case RPMSIGTAG_FILESIGNATURES: return "Filesignatures";
    case RPMSIGTAG_FILESIGNATURELENGTH: return "filesignaturelength";
    case RPMSIGTAG_VERITYSIGNATURES: return "veritysignatures";
    case RPMSIGTAG_VERITYSIGNATUREALGO: return "veritysignaturealgo";
    case RPMSIGTAG_RESERVED: return "Reserved";
    default:
	break;
    }
    return n;
}

struct rpmlead_s {
    unsigned char magic[4];
    unsigned char major;
    unsigned char minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;
    char reserved[16];
};

static int readlead(int fd)
{

    struct rpmlead_s lead;
    if (read(fd, &lead, 96) != 96) {
	fprintf(stderr, "failed to read lead\n");
	return 1;
    }

    printf("Lead magic: %hhx%hhx%hhx%hhx\n",
		lead.magic[0], lead.magic[1], lead.magic[2], lead.magic[3]);
    printf("Version %d.%d\n", lead.major, lead.minor);
    printf("Type: %hu\n", ntohs(lead.type));
    printf("Name: %s\n", lead.name);
    printf("Arch: %hu\n", ntohs(lead.osnum));
    printf("OS: %hu\n", ntohs(lead.osnum));
    printf("Sigtype: %hu\n", ntohs(lead.signature_type));
    printf("\n");

    return 0;
}

static void dumptag(struct entryInfo *entry, int sighdr, const char *pfx)
{
    const char *tagn;
    uint32_t tag = htonl(entry->tag);
    if (sighdr)
	tagn = sigTagName(tag);
    else
	tagn = rpmTagGetName(tag);
	
    printf("%stagno:  %4d (%s)\n", pfx, tag, tagn);
    printf("%stype:   %4d (%s)\n", pfx, htonl(entry->type),
		tagTypeNames[htonl(entry->type)]);
    printf("%soffset: %4d\n", pfx, htonl(entry->offset));
    printf("%scount:  %4d\n", pfx, htonl(entry->count));
}

static int readhdr(int fd, int sighdr, const char *msg)
{
    uint32_t intro[4], outro[4];
    uint32_t numEntries, numBytes, headerLen, indexLen;
    uint32_t *blob = NULL;
    uint8_t *dataStart;
    struct entryInfo *pe;
    struct entryInfo * entry = NULL;
    int rc = 1, i;

    if (read(fd, intro, sizeof(intro)) != sizeof(intro)) {
	fprintf(stderr, "header intro fail");
	goto exit;
    }

    printf("%s:\n", msg);
    
    printf("Header magic: %x (reserved: %x)\n", intro[0], intro[1]);

    numEntries = ntohl(intro[2]);
    numBytes = ntohl(intro[3]);
    indexLen = numEntries * sizeof(*entry);
    headerLen = indexLen + numBytes;

    blob = (uint32_t *)malloc(sizeof(numEntries) + sizeof(numBytes) + headerLen);
    blob[0] = htonl(numEntries);
    blob[1] = htonl(numBytes);

    pe = (struct entryInfo *) &(blob[2]);
    dataStart = (uint8_t *) (pe + numEntries);
    
    printf("Index entries: %d (%u bytes)\n", numEntries, indexLen);
    printf("Data size: %d bytes\n", numBytes);
    printf("Header size: %d bytes\n", headerLen);

    
    if (read(fd, blob+2, headerLen) != headerLen) {
	fprintf(stderr, "reading %d bytes of header fail\n", headerLen);
	goto exit;
    }

    /* signature header padding */
    if (sighdr) {
	int pad = (8 - (headerLen % 8)) % 8;
	printf("Padding: %d bytes", pad);
	if (read(fd, outro, pad) != pad) {
	    fprintf(stderr, "failed reading padding %d\n", pad);
	    goto exit;
	}
    }
    
{
    entry = (struct entryInfo *) (blob + 2);
    uint32_t tag = htonl(entry->tag);
    struct entryInfo _trailer, *trailer = &_trailer;
    int32_t toffset = 0;
    uint8_t *regionEnd = NULL;
    uint32_t ril = 0, rdl = 0;

    memset(trailer, 0, sizeof(*trailer));
    if (tag == 62 || tag == 63) {
	/* The trailer isn't guaranteed to be aligned, copy required */
	memcpy(trailer, dataStart + htonl(entry->offset), sizeof(*trailer));
	toffset = -htonl(trailer->offset);
	regionEnd = dataStart + toffset + 16;
	rdl = regionEnd - dataStart;
	ril = toffset/sizeof(*pe);
	printf("\nRegion entries %d\n", ril);
	printf("Region size %d\n", rdl);
	printf("Dribbles: %d\n", numEntries-ril);
    }
	    
    for (i = 0; i < numEntries; i++, entry++) {
	int in_region = (ril > 0 && i < ril);
	const char *marker = "";
	if (in_region) {
	    marker = (i < ril) ? "[region]" : "[dribble]";
	}
	tag = htonl(entry->tag);
	printf("\nTag #%d %s\n", i, marker);
	dumptag(entry, sighdr, "\t");

	if (tag == 62 || tag == 63) {
	    printf("\n\tregion trailer\n");
	    dumptag(trailer, sighdr, "\t\t");
	}
    }
}

    printf("\n");
    rc = 0;

exit:
    free(blob);
    return rc;
}

static int readpkg(int fd)
{
    int rc = 1;

    if (readlead(fd))
	return rc;

    /* signature header */
    if (readhdr(fd, 1, "Signature"))
	return rc;

    /* main header */
    if (readhdr(fd, 0, "Header"))
	return rc;

    /* payload ... */

    return 0;
}

int main(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
	int fd = open(argv[i], O_RDONLY);
	if (fd < 0)
	    continue;
	if (readpkg(fd))
	    fprintf(stderr, "bad package %s\n", argv[i]);
	close(fd);
    }
}
