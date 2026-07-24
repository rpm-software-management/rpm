/** \ingroup header
 * \file lib/package.c
 */

#include "system.h"

#include <string>
#include <mutex>
#include <set>
#include <vector>

#include <netinet/in.h>

#include <rpm/rpmlib.h>			/* XXX RPMSIGTAG, other sig stuff */
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmkeyring.h>
#include <rpm/rpmcrypto.h>
#include <rpm/rpmarchive.h>

#include "rpmlead.hh"
#include "signature.hh"
#include "rpmio_internal.hh"	/* fd digest bits */
#include "rpmalign.hh"		/* rpmAlignIsValid, rpmAlignUp */
#include "rpmpayload.hh"
#include "rpmlog_internal.hh"	/* rpmlogOnce */
#include "header_internal.hh"	/* XXX headerCheck */
#include "rpmvs.hh"

#include "debug.h"

typedef struct pkgdata_s *pkgdatap;

typedef void (*hdrvsmsg)(struct rpmsinfo_s *sinfo, pkgdatap pkgdata, const char *msg);

struct pkgdata_s {
    const char *fn;
    char *msg;
    uint64_t logDomain;
    rpmRC rc;
};

static struct taglate_s {
    rpmTagVal stag;
    rpmTagVal xtag;
    rpm_count_t count;
    int quirk;
} const xlateTags[] = {
    { RPMSIGTAG_SIZE, RPMTAG_SIGSIZE, 1, 0 },
    { RPMSIGTAG_PGP, RPMTAG_SIGPGP, 0, 0 },
    { RPMSIGTAG_MD5, RPMTAG_SIGMD5, 16, 0 },
    { RPMSIGTAG_GPG, RPMTAG_SIGGPG, 0, 0 },
    /* { RPMSIGTAG_PGP5, RPMTAG_SIGPGP5, 0, 0 }, */ /* long obsolete, dont use */
    { RPMSIGTAG_PAYLOADSIZE, RPMTAG_ARCHIVESIZE, 1, 1 },
    { RPMSIGTAG_FILESIGNATURES, RPMTAG_FILESIGNATURES, 0, 1 },
    { RPMSIGTAG_FILESIGNATURELENGTH, RPMTAG_FILESIGNATURELENGTH, 1, 1 },
    { RPMSIGTAG_VERITYSIGNATURES, RPMTAG_VERITYSIGNATURES, 0, 0 },
    { RPMSIGTAG_VERITYSIGNATUREALGO, RPMTAG_VERITYSIGNATUREALGO, 1, 0 },
    { RPMSIGTAG_SHA1, RPMTAG_SHA1HEADER, 1, 0 },
    { RPMSIGTAG_SHA256, RPMTAG_SHA256HEADER, 1, 0 },
    { RPMSIGTAG_SHA3_256, RPMTAG_SHA3_256HEADER, 1, 0 },
    { RPMSIGTAG_DSA, RPMTAG_DSAHEADER, 0, 0 },
    { RPMSIGTAG_RSA, RPMTAG_RSAHEADER, 0, 0 },
    { RPMSIGTAG_LONGSIZE, RPMTAG_LONGSIGSIZE, 1, 0 },
    { RPMSIGTAG_LONGARCHIVESIZE, RPMTAG_LONGARCHIVESIZE, 1, 0 },
    { RPMSIGTAG_OPENPGP, RPMTAG_OPENPGP, 0, 0 },
    { 0 }
};

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @param h		header (dest)
 * @param sigh		signature header (src)
 * @return		failing tag number, 0 on success
 */
static
rpmTagVal headerMergeLegacySigs(Header h, Header sigh, char **msg)
{
    const struct taglate_s *xl;
    struct rpmtd_s td;
    int rpmformat = headerGetNumber(h, RPMTAG_RPMFORMAT);

    for (xl = xlateTags; xl->stag; xl++) {
	/* There mustn't be one in the main header */
	if (headerIsEntry(h, xl->xtag)) {
	    /* In v3/v4 some tags may exist in either header, but never both */
	    if (rpmformat < 6 && xl->quirk && !headerIsEntry(sigh, xl->stag))
		continue;
	    goto exit;
	}
    }

    rpmtdReset(&td);
    for (xl = xlateTags; xl->stag; xl++) {
	/* Completely ignore legacy signature tags on v6 packages */
	if (rpmformat >= 6 && xl->stag >= HEADER_TAGBASE)
	    continue;
	if (headerGet(sigh, xl->stag, &td, HEADERGET_RAW|HEADERGET_MINMEM)) {
	    /* Translate legacy tags */
	    if (xl->stag != xl->xtag)
		td.tag = xl->xtag;
	    /* Ensure type and tag size match expectations */
	    if (td.type != rpmTagGetTagType(td.tag))
		break;
	    if (td.count < 1 || td.count > 16*1024*1024)
		break;
	    if (xl->count && td.count != xl->count)
		break;
	    if (!headerPut(h, &td, HEADERPUT_DEFAULT))
		break;
	    rpmtdFreeData(&td);
	}
    }
    rpmtdFreeData(&td);

exit:
    if (xl->stag) {
	rasprintf(msg, "invalid signature tag %s (%d)",
			rpmTagGetName(xl->xtag), xl->xtag);
    }

    return xl->stag;
}

static int handleHdrVS(struct rpmsinfo_s *sinfo, void *cbdata)
{
    struct pkgdata_s *pkgdata = (struct pkgdata_s *)cbdata;

    /* Remember actual return code, but don't override a previous failure */
    if (sortRC(sinfo->rc) > sortRC(pkgdata->rc))
	pkgdata->rc = sinfo->rc;

    return 1;
}


static int appendhdrmsg(struct rpmsinfo_s *sinfo, void *cbdata)
{
    struct pkgdata_s *pkgdata = (struct pkgdata_s *)cbdata;
    if (sinfo->rc != RPMRC_NOTFOUND) {
	char *msg = rpmsinfoMsg(sinfo);
	pkgdata->msg = rstrscat(&pkgdata->msg, "\n", msg, NULL);
	free(msg);
    }
    return 1;
}

rpmRC headerCheck(rpmts ts, const void * uh, size_t uc, char ** msg)
{
    rpmRC rc = RPMRC_FAIL;
    rpmVSFlags vsflags = rpmtsVSFlags(ts) | RPMVSF_NEEDPAYLOAD;
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    struct hdrblob_s blob;
    struct pkgdata_s pkgdata = {
	.fn = NULL,
	.msg = NULL,
	.logDomain = (uint64_t) ts,
	.rc = RPMRC_OK,
    };

    if (hdrblobInit(uh, uc, 0, 0, &blob, msg) == RPMRC_OK) {
	struct rpmvs_s *vs = rpmvsCreate(0, vsflags, keyring);
	rpmDigestBundle bundle = rpmDigestBundleNew();

	rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);

	rpmvsInit(vs, &blob, bundle);
	rpmvsInitRange(vs, RPMSIG_HEADER);
	hdrblobDigestUpdate(bundle, &blob);
	rpmvsFiniRange(vs, RPMSIG_HEADER);

	rpmvsVerify(vs, RPMSIG_VERIFIABLE_TYPE, handleHdrVS, &pkgdata);
	rpmvsForeach(vs, appendhdrmsg, &pkgdata);

	rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), uc);

	rc = pkgdata.rc;

	if (rc == RPMRC_OK && pkgdata.msg == NULL)
	    pkgdata.msg = xstrdup("Header sanity check: OK");

	if (msg)
	    *msg = pkgdata.msg;
	else
	    free(pkgdata.msg);

	rpmDigestBundleFree(bundle);
	rpmvsFree(vs);
    }

    rpmKeyringFree(keyring);

    return rc;
}

rpmRC rpmReadHeader(rpmts ts, FD_t fd, Header *hdrp, char ** msg)
{
    char *buf = NULL;
    struct hdrblob_s blob;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */

    if (hdrp)
	*hdrp = NULL;
    if (msg)
	*msg = NULL;

    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &buf) != RPMRC_OK)
	goto exit;

    /* OK, blob looks sane, load the header. */
    rc = hdrblobImport(&blob, 0, &h, &buf);
    
exit:
    if (hdrp && h && rc == RPMRC_OK)
	*hdrp = headerLink(h);
    headerFree(h);

    if (msg != NULL && *msg == NULL && buf != NULL) {
	*msg = buf;
    } else {
	free(buf);
    }

    return rc;
}

static
void applyRetrofits(Header h)
{
    int v3 = 0;
    /*
     * Make sure that either RPMTAG_SOURCERPM or RPMTAG_SOURCEPACKAGE
     * is set. Use a simple heuristic to find the type if both are unset.
     */
    if (!headerIsEntry(h, RPMTAG_SOURCERPM) && !headerIsEntry(h, RPMTAG_SOURCEPACKAGE)) {
	/* the heuristic needs the compressed file list */
	if (headerIsEntry(h, RPMTAG_OLDFILENAMES))
	    headerConvert(h, HEADERCONV_COMPRESSFILELIST);
	if (headerIsSourceHeuristic(h)) {
	    /* Retrofit RPMTAG_SOURCEPACKAGE to srpms for compatibility */
	    uint32_t one = 1;
	    headerPutUint32(h, RPMTAG_SOURCEPACKAGE, &one, 1);
	} else {
	    /*
	     * Make sure binary rpms have RPMTAG_SOURCERPM set as that's
	     * what we use for differentiating binary vs source elsewhere.
	     */
	    headerPutString(h, RPMTAG_SOURCERPM, "(none)");
	}
    }

    /*
     * Convert legacy headers on the fly. Not having immutable region
     * equals a truly ancient package, do full retrofit. OTOH newer
     * packages might have been built with --nodirtokens, test and handle
     * the non-compressed filelist case separately.
     */
    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	v3 = 1;
	headerConvert(h, HEADERCONV_RETROFIT_V3);
    } else if (headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	headerConvert(h, HEADERCONV_COMPRESSFILELIST);
	v3 = 1;
    }
    if (v3) {
	char *s = headerGetAsString(h, RPMTAG_NEVRA);
	rpmlog(RPMLOG_WARNING, _("RPM v3 packages are deprecated: %s\n"), s);
	free(s);
    }
}

static int loghdrmsg(struct rpmsinfo_s *sinfo, void *cbdata)
{
    const struct pkgdata_s *pkgdata = (const struct pkgdata_s *)cbdata;
    int lvl = RPMLOG_DEBUG;
    int once = 0;
    int log = 1;

    switch (sinfo->rc) {
    case RPMRC_NOTFOUND:	/* Signature/digest not present. */
	log = 0;
	break;
    case RPMRC_NOTTRUSTED:	/* Signature is OK, but key is not trusted. */
    case RPMRC_OK:		/* Signature is OK. */
	break;
    case RPMRC_NOKEY:		/* Public key is unavailable. */
	lvl = RPMLOG_WARNING;
	once = 1;
	break;
    default:
    case RPMRC_FAIL:		/* Signature does not verify. */
	lvl = RPMLOG_ERR;
	break;
    }

    if (log) {
	char *msg = rpmsinfoMsg(sinfo);
	if (once) {
	    rpmlogOnce(pkgdata->logDomain, sinfo->keyid, lvl,
			"%s: %s\n", pkgdata->fn, msg);
	} else {
	    rpmlog(lvl, "%s: %s\n", pkgdata->fn, msg);
	}
	free(msg);
    }

    return 1;
}

rpmRC rpmReadPackageFile(rpmts ts, FD_t fd, const char * fn, Header * hdrp)
{
    char *msg = NULL;
    Header h = NULL;
    Header sigh = NULL;
    hdrblob blob = NULL;
    hdrblob sigblob = NULL;
    rpmVSFlags vsflags = rpmtsVSFlags(ts) | RPMVSF_NEEDPAYLOAD;
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    struct rpmvs_s *vs = rpmvsCreate(0, vsflags, keyring);
    struct pkgdata_s pkgdata = {
	.fn = fn ? fn : Fdescr(fd),
	.msg = NULL,
	.logDomain = (uint64_t) ts,
	.rc = RPMRC_OK,
    };

    /* XXX: lots of 3rd party software relies on the behavior */
    if (hdrp)
	*hdrp = NULL;

    rpmRC rc = rpmpkgRead(vs, fd, &sigblob, &blob, &msg);
    if (rc)
	goto exit;

    /* Actually all verify discovered signatures and digests */
    rc = RPMRC_FAIL;
    rpmvsVerify(vs, RPMSIG_VERIFIABLE_TYPE, handleHdrVS, &pkgdata);
    rpmvsForeach(vs, loghdrmsg, &pkgdata);

    /* Preserve traditional behavior for now: only failure prevents read */
    if (pkgdata.rc != RPMRC_FAIL) {
	/* Finally import the headers and do whatever required retrofits etc */
	if (hdrp) {
	    if (hdrblobImport(sigblob, 0, &sigh, &msg))
		goto exit;
	    if (hdrblobImport(blob, 0, &h, &msg))
		goto exit;

	    /* Append (and remap) signature tags to the metadata. */
	    if (headerMergeLegacySigs(h, sigh, &msg))
		goto exit;
	    applyRetrofits(h);

	    /* Bump reference count for return. */
	    *hdrp = headerLink(h);
	}
	rc = RPMRC_OK;
    }

    /* If there was a "substatus" (NOKEY in practise), return that instead */
    if (rc == RPMRC_OK && pkgdata.rc)
	rc = pkgdata.rc;

exit:
    if (rc && msg)
	rpmlog(RPMLOG_ERR, "%s: %s\n", Fdescr(fd), msg);
    hdrblobFree(sigblob);
    hdrblobFree(blob);
    headerFree(sigh);
    headerFree(h);
    rpmKeyringFree(keyring);
    rpmvsFree(vs);
    free(msg);

    return rc;
}

#define UNCOMPRESS_BUFSIZE (128*1024)

static int distinctRegularFiles(FD_t a, FD_t b)
{
    struct stat ast, bst;

    return fdIsPlain(a) && fdIsPlain(b) &&
	   Ftell(a) >= 0 && Ftell(b) >= 0 &&
	   fstat(Fileno(a), &ast) == 0 && S_ISREG(ast.st_mode) &&
	   fstat(Fileno(b), &bst) == 0 && S_ISREG(bst.st_mode) &&
	   (ast.st_dev != bst.st_dev || ast.st_ino != bst.st_ino);
}

static int copyBytes(FD_t src, FD_t dst, off_t start, off_t size)
{
    char buf[UNCOMPRESS_BUFSIZE];

    if (Fseek(src, start, SEEK_SET) < 0)
	return -1;
    while (size > 0) {
	size_t n = size > (off_t)sizeof(buf) ? sizeof(buf) : (size_t)size;
	if (Fread(buf, 1, n, src) != (ssize_t)n ||
	    Fwrite(buf, 1, n, dst) != (ssize_t)n)
	    return -1;
	size -= n;
    }
    return 0;
}

/*
 * Whole-package signatures, digests and sizes describe the compressed
 * representation and cannot be retained. Header-only signatures remain valid
 * because the main header is copied byte-for-byte.
 */
static Header materializedSignatureHeader(Header sigh)
{
    std::vector<rpmTagVal> stale;
    rpmTagVal tag;
    int bad = 0;
    int legacy = 0;
    int header = 0;

    rpmUnwrapSignature(&sigh);
    HeaderIterator hi = headerInitIterator(sigh);
    while (!bad && (tag = headerNextTag(hi)) != RPMTAG_NOT_FOUND) {
	switch (tag) {
	case RPMTAG_HEADERIMAGE:
	case RPMTAG_HEADERSIGNATURES:
	case RPMTAG_HEADERIMMUTABLE:
	case RPMTAG_HEADERREGIONS:
	case RPMSIGTAG_RESERVEDSPACE:
	case RPMSIGTAG_BADSHA1_1:
	case RPMSIGTAG_BADSHA1_2:
	case RPMTAG_PUBKEYS:
	case RPMSIGTAG_SHA1:
	case RPMSIGTAG_SHA256:
	case RPMSIGTAG_FILESIGNATURES:
	case RPMSIGTAG_FILESIGNATURELENGTH:
	case RPMSIGTAG_VERITYSIGNATURES:
	case RPMSIGTAG_VERITYSIGNATUREALGO:
	case RPMSIGTAG_SHA3_256:
	case RPMSIGTAG_RESERVED:
	    break;
	case RPMSIGTAG_SIZE:
	case RPMSIGTAG_LEMD5_1:
	case RPMSIGTAG_LEMD5_2:
	case RPMSIGTAG_MD5:
	case RPMSIGTAG_PAYLOADSIZE:
	case RPMSIGTAG_LONGSIZE:
	case RPMSIGTAG_LONGARCHIVESIZE:
	    stale.push_back(tag);
	    break;
	case RPMSIGTAG_PGP:
	case RPMSIGTAG_GPG:
	case RPMSIGTAG_PGP5:
	    legacy = 1;
	    stale.push_back(tag);
	    break;
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_RSA:
	case RPMSIGTAG_OPENPGP:
	    header = 1;
	    break;
	default:
	    rpmlog(RPMLOG_ERR,
		   _("unsupported signature header tag %d\n"), tag);
	    bad = 1;
	    break;
	}
    }
    headerFreeIterator(hi);
    if (!bad && legacy && !header) {
	rpmlog(RPMLOG_ERR,
	       _("legacy header+payload signature cannot be preserved\n"));
	bad = 1;
    }
    if (bad) {
	headerFree(sigh);
	return NULL;
    }
    for (auto tag : stale)
	headerDel(sigh, tag);
    return headerReload(sigh, RPMTAG_HEADERSIGNATURES);
}

/* Copy a canonical uncompressed payload and verify its signed ALT digest. */
static int copyPayloadAlt(FD_t src, FD_t dst, Header h, rpmVSFlags vsflags)
{
    static const struct {
	rpmTagVal primary;
	rpmTagVal alt;
	int algo;
	rpmVSFlags disable;
    } digests[] = {
	{ RPMTAG_PAYLOADSHA3_256, RPMTAG_PAYLOADSHA3_256ALT,
	  RPM_HASH_SHA3_256,
	  RPMVSF_NOSHA3_256PAYLOAD },
	{ RPMTAG_PAYLOADSHA512, RPMTAG_PAYLOADSHA512ALT, RPM_HASH_SHA512,
	  RPMVSF_NOSHA512PAYLOAD },
	{ RPMTAG_PAYLOADSHA256, RPMTAG_PAYLOADSHA256ALT, RPM_HASH_SHA256,
	  RPMVSF_NOSHA256PAYLOAD },
	{ 0, 0, 0, 0 }
    };
    struct digest_s {
	rpmTagVal alt;
	std::vector<std::string> expected;
    };
    std::vector<digest_s> active;
    rpm_loff_t size = 0;
    rpm_loff_t expectedSize = 0;
    int haveSize = headerIsEntry(h, RPMTAG_PAYLOADSIZEALT);
    char buf[UNCOMPRESS_BUFSIZE];
    ssize_t n;

    for (int i = 0; digests[i].alt; i++) {
	if (vsflags & digests[i].disable)
	    continue;
	std::vector<std::string> expected;
	struct rpmtd_s td;

	if (headerGet(h, digests[i].alt, &td, HEADERGET_MINMEM)) {
	    while (rpmtdNext(&td) >= 0) {
		const char *value = rpmtdGetString(&td);
		if (value == NULL) {
		    rpmtdFreeData(&td);
		    return -1;
		}
		expected.push_back(value);
	    }
	    rpmtdFreeData(&td);
	}
	if (expected.empty()) {
	    if (headerIsEntry(h, digests[i].primary)) {
		rpmlog(RPMLOG_ERR,
		       _("source package has no matching ALT payload digest\n"));
		return -1;
	    }
	    continue;
	}
	/* The digest contexts are owned and reclaimed by src. */
	fdInitDigestID(src, digests[i].algo, digests[i].alt, 0);
	active.push_back({digests[i].alt, expected});
    }
    if (active.empty()) {
	rpmlog(RPMLOG_ERR, _("source package has no usable ALT payload digest\n"));
	return -1;
    }
    if (haveSize)
	expectedSize = headerGetNumber(h, RPMTAG_PAYLOADSIZEALT);

    while ((n = Fread(buf, 1, sizeof(buf), src)) > 0) {
	if (haveSize && (size > expectedSize ||
	    (rpm_loff_t)n > expectedSize - size)) {
	    rpmlog(RPMLOG_ERR, _("uncompressed payload ALT size mismatch\n"));
	    return -1;
	}
	if (Fwrite(buf, 1, n, dst) != n)
	    return -1;
	size += n;
    }
    if (n < 0 || Ferror(src) || Ferror(dst))
	return -1;

    for (const auto &digest : active) {
	void *data = NULL;
	fdFiniDigest(src, digest.alt, &data, NULL, 1);
	if (data == NULL)
	    return -1;
	std::string actual((const char *)data);
	free(data);
	for (const auto &expected : digest.expected) {
	    if (strcasecmp(actual.c_str(), expected.c_str())) {
		rpmlog(RPMLOG_ERR,
		       _("uncompressed payload ALT digest mismatch\n"));
		return -1;
	    }
	}
    }
    if (haveSize && expectedSize != size) {
	rpmlog(RPMLOG_ERR, _("uncompressed payload ALT size mismatch\n"));
	return -1;
    }
    return 0;
}

/* Decompress the payload of src into fdo, verifying its ALT digest. */
static rpmRC copyPayload(rpmts ts, FD_t src, FD_t fdo, Header h,
			 const struct rpmPayloadInfo *payload)
{
    if (Fseek(src, payload->start, SEEK_SET) < 0)
	return RPMRC_FAIL;
    std::string rpmio_flags = std::string("r.") + payload->io;
    FD_t cfd = Fdopen(fdDup(Fileno(src)), rpmio_flags.c_str());
    if (cfd == NULL || Ferror(cfd)) {
	rpmlog(RPMLOG_ERR, _("cannot open payload: %s\n"), Fstrerror(cfd));
	if (cfd)
	    Fclose(cfd);
	return RPMRC_FAIL;
    }
    int rc = copyPayloadAlt(cfd, fdo, h, rpmtsVSFlags(ts));
    Fclose(cfd);
    return rc ? RPMRC_FAIL : RPMRC_OK;
}

/* Headers are owned by the caller, which frees them even on failure. */
static rpmRC uncompressPackage(rpmts ts, FD_t src, FD_t fdo,
			       Header *hp, Header *sighp)
{
    /* Read the source package; leaves it at the payload. */
    rpmRC rc = rpmReadPackageFile(ts, src, NULL, hp);
    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
	break;
    default:
	return rc;
    }
    Header h = *hp;
    off_t payloadStart = Ftell(src);
    if (payloadStart < 0 || Fseek(src, RPMLEAD_SIZE, SEEK_SET) < 0 ||
	rpmReadSignature(src, sighp, NULL) != RPMRC_OK)
	return RPMRC_FAIL;
    off_t headerStart = Ftell(src);
    if (headerStart < RPMLEAD_SIZE || headerStart > payloadStart)
	return RPMRC_FAIL;
    *sighp = materializedSignatureHeader(*sighp);
    if (*sighp == NULL)
	return RPMRC_FAIL;

    /*
     * Content alignment is an optimization recorded by the build: without it
     * the payload is materialized with no framing and cannot be cloned from,
     * only copied.
     */
    uint64_t source_align = headerGetNumber(h, RPMTAG_PAYLOADALIGNMENT);
    if (!rpmAlignIsValid(source_align))
	source_align = 1;

    struct rpmPayloadInfo payload;
    if (Fseek(src, payloadStart, SEEK_SET) < 0 ||
	rpmPayloadProbe(src, h, &payload))
	return RPMRC_FAIL;

    /*
     * The output is caller-owned and unspecified on failure. A caller needing
     * atomic replacement can materialize to a temporary file and rename it.
     */
    if (Fseek(fdo, 0, SEEK_SET) < 0 || ftruncate(Fileno(fdo), 0) ||
	copyBytes(src, fdo, 0, RPMLEAD_SIZE) ||
	rpmWriteSignature(fdo, *sighp) ||
	copyBytes(src, fdo, headerStart, payloadStart - headerStart))
	return RPMRC_FAIL;

    payloadStart = Ftell(fdo);
    off_t archiveStart = rpmAlignUp(payloadStart, source_align);
    if (payloadStart < 0 || archiveStart < payloadStart ||
	fdWriteZeros(fdo, archiveStart - payloadStart))
	return RPMRC_FAIL;

    if (copyPayload(ts, src, fdo, h, &payload) != RPMRC_OK || Fflush(fdo))
	return RPMRC_FAIL;

    off_t end = Ftell(fdo);
    if (end < 0 || ftruncate(Fileno(fdo), end))
	return RPMRC_FAIL;
    return RPMRC_OK;
}

rpmRC rpmUncompressPackage(rpmts ts, FD_t fdi, FD_t fdo)
{
    if (!distinctRegularFiles(fdi, fdo)) {
	rpmlog(RPMLOG_ERR,
	       _("package materialization requires distinct unfiltered regular files\n"));
	return RPMRC_FAIL;
    }
    FD_t src = fdDup(Fileno(fdi));
    if (src == NULL || Fseek(src, 0, SEEK_SET) < 0) {
	if (src)
	    Fclose(src);
	return RPMRC_FAIL;
    }
    Header h = NULL;
    Header sigh = NULL;
    rpmRC rc = uncompressPackage(ts, src, fdo, &h, &sigh);
    headerFree(sigh);
    headerFree(h);
    Fclose(src);
    return rc;
}
