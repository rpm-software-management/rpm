#include "system.h"
#include "rpmio_internal.h"
#include "misc.h"
#include "popt.h"
#include "debug.h"

static int rpmSlurp(const char * fn, const byte ** bp, ssize_t * blenp)
{
    static ssize_t blenmax = (8 * BUFSIZ);
    ssize_t blen = 0;
    byte * b = NULL;
    ssize_t size;
    FD_t fd;
    int rc = 0;

    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rc = 2;
	goto exit;
    }

    size = fdSize(fd);
    blen = (size >= 0 ? size : blenmax);
    if (blen) {
	int nb;
	b = xmalloc(blen+1);
	b[0] = '\0';
	nb = Fread(b, sizeof(*b), blen, fd);
	if (Ferror(fd) || (size > 0 && nb != blen)) {
	    rc = 1;
	    goto exit;
	}
	if (blen == blenmax && nb < blen) {
	    blen = nb;
	    b = xrealloc(b, blen+1);
	}
	b[blen] = '\0';
    }

exit:
    if (fd) (void) Fclose(fd);
	
    if (rc) {
	if (b) free(b);
	b = NULL;
	blen = 0;
    }

    if (bp) *bp = b;
    else if (b) free(b);

    if (blenp) *blenp = blen;

    return rc;
}

static int rpmReadPgpPkt(const char * fn, const byte ** pkt, size_t * pktlen)
{
    const byte * b = NULL;
    ssize_t blen;
    const char * enc = NULL;
    const char * crcenc = NULL;
    byte * dec;
    byte * crcdec;
    size_t declen;
    size_t crclen;
    uint32 crcpkt, crc;
    const char * armortype = NULL;
    char * t, * te;
    int pstate = 0;
    int ec = 1;	/* XXX assume failure */
    int rc;

    rc = rpmSlurp(fn, &b, &blen);
    if (rc || b == NULL || blen <= 0)
	goto exit;

    if (pgpIsPkt(b)) {
	ec = 0;
	goto exit;
    }

#define	TOKEQ(_s, _tok)	(!strncmp((_s), (_tok), sizeof(_tok)-1))

    for (t = (char *)b; t && *t; t = te) {
	if ((te = strchr(t, '\n')) == NULL)
		te = t + strlen(t);
	else
		te++;

	switch (pstate) {
	case 0:
	    armortype = NULL;
	    if (!TOKEQ(t, "-----BEGIN PGP "))
		continue;
	    t += sizeof("-----BEGIN PGP ")-1;

	    rc = pgpValTok(pgpArmorTbl, t, te);
	    if (rc < 0)
		goto exit;
	    armortype = t;

	    t = te - (sizeof("-----\n")-1);
	    if (!TOKEQ(t, "-----\n"))
		continue;
	    *t = '\0';
	    pstate++;
	    break;
	case 1:
	    enc = NULL;
	    rc = pgpValTok(pgpArmorKeyTbl, t, te);
	    if (rc >= 0)
		continue;
	    if (*t != '\n') {
		pstate = 0;
		continue;
	    }
	    enc = te;		/* Start of encoded packets */
	    pstate++;
	    break;
	case 2:
	    crcenc = NULL;
	    if (*t != '=')
		continue;
	    *t++ = '\0';	/* Terminate encoded packets */
	    crcenc = t;		/* Start of encoded crc */
	    pstate++;
	    break;
	case 3:
	    pstate = 0;
	    if (!TOKEQ(t, "-----END PGP "))
		goto exit;
	    *t = '\0';		/* Terminate encoded crc */
	    t += sizeof("-----END PGP ")-1;

	    if (armortype == NULL) /* XXX can't happen */
		continue;
	    rc = strncmp(t, armortype, strlen(armortype));
	    if (rc)
		continue;

	    t = te - (sizeof("-----\n")-1);
	    if (!TOKEQ(t, "-----\n"))
		goto exit;

	    if (b64decode(crcenc, (void **)&crcdec, &crclen) != 0)
		continue;
	    crcpkt = pgpGrab(crcdec, crclen);
	    free(crcdec);
	    if (b64decode(enc, (void **)&dec, &declen) != 0)
		goto exit;
	    crc = pgpCRC(dec, declen);
	    if (crcpkt != crc)
		goto exit;
	    free((void *)b);
	    b = dec;
	    blen = declen;
	    ec = 0;
	    goto exit;
	    /*@notreached@*/ break;
	}
    }

exit:
    if (ec == 0 && pkt)
	*pkt = b;
    else if (b != NULL)
	free((void *)b);
    if (pktlen)
	*pktlen = blen;
    return rc;
}

static int printing = 0;
static int _debug = 0;

static struct poptOption optionsTable[] = {
 { "print", 'p', POPT_ARG_VAL,	&printing, 1,		NULL, NULL },
 { "noprint", 'n', POPT_ARG_VAL, &printing, 0,		NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main (int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    rpmDigest dig;
    const byte * pkt = NULL;
    ssize_t pktlen;
    const char ** args;
    const char * fn;
    int rc = 0;

    while ((rc = poptGetNextOpt(optCon)) > 0)
	;

    if ((args = poptGetArgs(optCon)) != NULL)
    while ((fn = *args++) != NULL) {
	rc = rpmReadPgpPkt(fn, &pkt, &pktlen);
	if (rc || pkt == NULL || pktlen <= 0)
	    continue;

fprintf(stderr, "===================== %s\n", fn);
	dig = xcalloc(1, sizeof(*dig));
	(void) pgpPrtPkts(pkt, pktlen, dig, printing);
	free((void *)pkt);
	pkt = NULL;
	free((void *)dig);
	dig = NULL;
    }

    return rc;
}
