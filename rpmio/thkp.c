#include "system.h"

#include <rpmio_internal.h>
#include <rpmmacro.h>
#include <rpmmessages.h>
#include <popt.h>

#include "debug.h"

static int _debug = 0;
static int _printing = 0;

int noNeon;

#if 0
#define	HKPPATH		"hkp://pgp.mit.edu:11371/pks/lookup?op=get&search=0xF5C75256"
#else
#if 0
#define	HKPPATH		"hkp://pgp.mit.edu"
#else
#define	HKPPATH		"hkp://sks.keyserver.penguin.de"
#endif
#endif
static char * hkppath = HKPPATH;

static unsigned int keyids[] = {
#if 0
	0xc2b079fc, 0xf5c75256,
	0x94cd5742, 0xe418e3aa,
	0xb44269d0, 0x4f2a6fd2,
	0xda84cbd4, 0x30c9ecf8,
	0x29d5ba24, 0x8df56d05,
	0xa520e8f1, 0xcba29bf9,
	0x219180cd, 0xdb42a60e,
	0xfd372689, 0x897da07a,
	0xe1385d4e, 0x1cddbca9,
	0xb873641b, 0x2039b291,
#endif
	0x58e727c4, 0xc621be0f,
	0
};

static int readKeys(const char * uri)
{
    unsigned int * kip;
    const byte * pkt;
    ssize_t pktlen;
    byte keyid[8];
    char fn[BUFSIZ];
    pgpDig dig;
    int rc;
    int ec = 0;

    dig = pgpNewDig();
    for (kip = keyids; *kip; kip += 2) {
	pgpArmor pa;

	sprintf(fn, "%s/pks/lookup?op=get&search=0x%08x%08x", uri, kip[0], kip[1]);
fprintf(stderr, "======================= %s\n", fn);
	pkt = NULL;
	pktlen = 0;
	pa = pgpReadPkts(fn, &pkt, &pktlen);
	if (pa == PGPARMOR_ERROR || pa == PGPARMOR_NONE
         || pkt == NULL || pktlen <= 0)
        {
            ec++;
            continue;
        }

	rc = pgpPrtPkts(pkt, pktlen, dig, _printing);
	if (rc)
	    ec++;
#if 0
fprintf(stderr, "%s\n", pgpHexStr(pkt, pktlen));
#endif
	if (!pgpPubkeyFingerprint(pkt, pktlen, keyid))
fprintf(stderr, "KEYID: %08x %08x\n", pgpGrab(keyid, 4), pgpGrab(keyid+4, 4));


	pgpCleanDig(dig);

	free((void *)pkt);
	pkt = NULL;
    }
    dig = pgpFreeDig(dig);

    return ec;
}

static struct poptOption optionsTable[] = {
 { "print", 'p', POPT_ARG_VAL,  &_printing, 1,		NULL, NULL },
 { "noprint", 'n', POPT_ARG_VAL, &_printing, 0,		NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
 { "davdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_dav_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "noneon", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &noNeon, 1,
	N_("disable use of libneon for HTTP"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    int rc;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    readKeys(hkppath);

/*@i@*/ urlFreeCache();

    return 0;
}
