#include <stdio.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmts.h>

int main(int argc, char *argv[])
{
    uint8_t *pkt = NULL;
    uint8_t *pkti = NULL;
    size_t pktlen = 0;
    size_t certlen = 0;
    ssize_t blen = 128;
    uint8_t *buf = NULL;
    int rc = 1;
    int nkeys = 0;
    rpmts ts = rpmtsCreate();
    FD_t fd = NULL;
    const char *fn = argv[1];

    if (rpmReadConfigFiles(NULL, NULL)) {
	fprintf(stderr, "failed to read config\n");
	goto exit;
    }

    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL) {
	fprintf(stderr, "failed to open %s\n", fn);
	goto exit;
    }

    blen = fdSize(fd);
    if (blen < 1) {
	fprintf(stderr, "invalid file size %zu\n", blen);
	goto exit;
    }

    buf = rcalloc(blen+1, 1);
    if (Fread(buf, 1, blen, fd) != blen) {
	fprintf(stderr, "failed to read %s: %m\n", fn);
	goto exit;
    }

    if (pgpParsePkts((char *)buf, &pkt, &pktlen) != PGPARMOR_PUBKEY) {
	fprintf(stderr, "failed to parse key\n");
	goto exit;
    }

    pkti = pkt;
    while (pktlen > 0) {
	if (pgpPubKeyCertLen(pkti, pktlen, &certlen))
	    break;

	rpmRC irc = rpmtsImportPubkey(ts, pkti, certlen);
	printf("%s key %d: %d\n", argv[1], ++nkeys, irc);
	pkti += certlen;
	pktlen -= certlen;

	/* If we get at least one key imported, consider it a success */
	if (irc == RPMRC_OK)
	    rc = 0;
    }

exit:
    Fclose(fd);
    free(buf);
    free(pkt);
    rpmtsFree(ts);
    return rc;
}
