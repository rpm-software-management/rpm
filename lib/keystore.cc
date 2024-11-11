#include "system.h"

#include <string>

#include <fcntl.h>

#include <rpm/header.h>
#include <rpm/rpmbase64.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmts.h>
#include <rpm/rpmtypes.h>

#include "rpmts_internal.hh"

#include "debug.h"

using std::string;
using namespace rpm;

static int makePubkeyHeader(rpmts ts, rpmPubkey key, Header * hdrp);

static rpmRC load_keys_from_glob(rpmtxn txn, rpmKeyring keyring, string glob)
{
    ARGV_t files = NULL;
    /* XXX TODO: deal with chroot path issues */
    char *pkpath = rpmGetPath(rpmtxnRootDir(txn), glob.c_str(), NULL);

    rpmlog(RPMLOG_DEBUG, "loading keyring from pubkeys in %s\n", pkpath);
    if (rpmGlob(pkpath, NULL, &files)) {
	rpmlog(RPMLOG_DEBUG, "couldn't find any keys in %s\n", pkpath);
	goto exit;
    }

    for (char **f = files; *f; f++) {
	rpmPubkey key = rpmPubkeyRead(*f);

	if (!key) {
	    rpmlog(RPMLOG_ERR, _("%s: reading of public key failed.\n"), *f);
	    continue;
	}

	if (rpmKeyringAddKey(keyring, key) == 0) {
	    rpmlog(RPMLOG_DEBUG, "Loaded key %s\n", *f);
	}
	rpmPubkeyFree(key);
    }
exit:
    free(pkpath);
    argvFree(files);
    return RPMRC_OK;
}

static rpmRC write_key_to_disk(rpmPubkey key, string & dir, string & filename, int replace, rpmFlags flags)
{
    rpmRC rc = RPMRC_FAIL;
    char *keyval = rpmPubkeyArmorWrap(key);
    string tmppath = "";
    string path = dir + "/" + filename;

    if (replace) {
	tmppath =  dir + "/" + filename + ".new";
	unlink(tmppath.c_str());
    }

    if (rpmMkdirs(NULL, dir.c_str())) {
	rpmlog(RPMLOG_ERR, _("failed to create keyring directory %s: %s\n"),
	       path.c_str(), strerror(errno));
	goto exit;
    }

    if (FD_t fd = Fopen(replace ? tmppath.c_str() : path.c_str(), "wx")) {
	size_t keylen = strlen(keyval);
	if (Fwrite(keyval, 1, keylen, fd) == keylen)
	    rc = RPMRC_OK;
	Fclose(fd);
    }
    if (!rc && replace && rename(tmppath.c_str(), path.c_str()) != 0)
	rc = RPMRC_FAIL;

    if (rc) {
	rpmlog(RPMLOG_ERR, _("failed to import key: %s: %s\n"),
	       replace ? tmppath.c_str() : path.c_str(), strerror(errno));
	if (replace)
	    unlink(tmppath.c_str());
    }

exit:
    free(keyval);
    return rc;
}


/*****************************************************************************/

rpmRC keystore_fs::load_keys(rpmtxn txn, rpmKeyring keyring)
{
    return load_keys_from_glob(txn, keyring, "%{_keyringpath}/*.key");
}

rpmRC keystore_fs::delete_key(rpmtxn txn, const string & keyid, const string & newname)
{
    rpmRC rc = RPMRC_NOTFOUND;
    string keyglob = "gpg-pubkey-" + keyid + "*.key";
    ARGV_t files = NULL;
    char *pkpath = rpmGenPath(rpmtxnRootDir(txn), "%{_keyringpath}/", keyglob.c_str());
    if (rpmGlob(pkpath, NULL, &files) == 0) {
	char **f;
	for (f = files; *f; f++) {
	    char *bf = strrchr(*f, '/');
	    if (newname.empty() || (bf && strcmp(bf + 1, newname.c_str()) != 0))
		rc = unlink(*f) ? RPMRC_FAIL : RPMRC_OK;
	}
	argvFree(files);
    }
    free(pkpath);
    return rc;
}

rpmRC keystore_fs::delete_key(rpmtxn txn, rpmPubkey key)
{
    return delete_key(txn, rpmPubkeyFingerprintAsHex(key));
}

rpmRC keystore_fs::import_key(rpmtxn txn, rpmPubkey key, int replace, rpmFlags flags)
{
    rpmRC rc = RPMRC_FAIL;
    string fp = rpmPubkeyFingerprintAsHex(key);
    string keyfmt = "gpg-pubkey-" + fp + ".key";
    char *dir = rpmGenPath(rpmtxnRootDir(txn), "%{_keyringpath}/", NULL);
    string dirstr = dir;

    rc = write_key_to_disk(key, dirstr, keyfmt, replace, flags);

    if (!rc && replace) {
	/* find and delete the old pubkey entry */
	if (delete_key(txn, fp, keyfmt) == RPMRC_NOTFOUND) {
	    /* make sure an old, short keyid version gets removed */
	    delete_key(txn, fp.substr(32), keyfmt);
	}
    }

    free(dir);
    return rc;
}

rpmRC keystore_rpmdb::load_keys(rpmtxn txn, rpmKeyring keyring)
{
    Header h;
    rpmdbMatchIterator mi;

    rpmlog(RPMLOG_DEBUG, "loading keyring from rpmdb\n");
    mi = rpmtsInitIterator(rpmtxnTs(txn), RPMDBI_NAME, "gpg-pubkey", 0);
    while ((h = rpmdbNextIterator(mi)) != NULL) {
	struct rpmtd_s pubkeys;
	const char *key;

	if (!headerGet(h, RPMTAG_PUBKEYS, &pubkeys, HEADERGET_MINMEM))
	   continue;

	char *nevr = headerGetAsString(h, RPMTAG_NEVR);
	while ((key = rpmtdNextString(&pubkeys))) {
	    uint8_t *pkt;
	    size_t pktlen;

	    if (rpmBase64Decode(key, (void **) &pkt, &pktlen) == 0) {
		rpmPubkey key = rpmPubkeyNew(pkt, pktlen);

		if (key) {
		    if (rpmKeyringAddKey(keyring, key) == 0) {
			rpmlog(RPMLOG_DEBUG, "Loaded key %s\n", nevr);
		    }
		    rpmPubkeyFree(key);
		}
		free(pkt);
	    }
	}
	free(nevr);
	rpmtdFreeData(&pubkeys);
    }
    rpmdbFreeIterator(mi);

    return RPMRC_OK;
}

rpmRC keystore_rpmdb::delete_key(rpmtxn txn, const string & keyid, unsigned int newinstance)
{
    rpmts ts = rpmtxnTs(txn);
    if (rpmtsOpenDB(ts, (O_RDWR|O_CREAT)))
	return RPMRC_FAIL;

    rpmRC rc = RPMRC_NOTFOUND;
    unsigned int otherinstance = 0;
    Header oh;
    string label = "gpg-pubkey-" + keyid;
    rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMDBI_LABEL, label.c_str(), 0);

    while (otherinstance == 0 && (oh = rpmdbNextIterator(mi)) != NULL)
	if (headerGetInstance(oh) != newinstance)
	    otherinstance = headerGetInstance(oh);
    rpmdbFreeIterator(mi);
    if (otherinstance) {
	rc = rpmdbRemove(rpmtsGetRdb(ts), otherinstance) ?
		RPMRC_FAIL : RPMRC_OK;
    }

    return rc;
}

rpmRC keystore_rpmdb::delete_key(rpmtxn txn, rpmPubkey key)
{
    return delete_key(txn, rpmPubkeyFingerprintAsHex(key));
}

rpmRC keystore_rpmdb::import_key(rpmtxn txn, rpmPubkey key, int replace, rpmFlags flags)
{
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;

    if (makePubkeyHeader(rpmtxnTs(txn), key, &h) != 0)
	return rc;

    rc = rpmtsImportHeader(txn, h, 0);

    if (!rc && replace) {
	/* find and delete the old pubkey entry */
	unsigned int newinstance = headerGetInstance(h);
	char *keyid = headerFormat(h, "%{version}", NULL);
	if (delete_key(txn, keyid, newinstance) == RPMRC_NOTFOUND) {
	    /* make sure an old, short keyid version gets removed */
	    delete_key(txn, keyid+32, newinstance);
	}
	free(keyid);
    }
    headerFree(h);

    return rc;
}

static void addGpgProvide(Header h, const char *n, const char *v)
{
    rpmsenseFlags pflags = (RPMSENSE_KEYRING|RPMSENSE_EQUAL);
    char * nsn = rstrscat(NULL, "gpg(", n, ")", NULL);

    headerPutString(h, RPMTAG_PROVIDENAME, nsn);
    headerPutString(h, RPMTAG_PROVIDEVERSION, v);
    headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &pflags, 1);

    free(nsn);
}

struct pgpdata_s {
    const char *fingerprint;
    const char *signid;
    char *timestr;
    char *verid;
    const char *userid;
    const char *shortid;
    uint32_t time;
};

static void initPgpData(rpmPubkey key, struct pgpdata_s *pd)
{
    pgpDigParams pubp = rpmPubkeyPgpDigParams(key);
    memset(pd, 0, sizeof(*pd));
    pd->fingerprint = rpmPubkeyFingerprintAsHex(key);
    pd->signid = rpmPubkeyKeyIDAsHex(key);
    pd->shortid = pd->signid + 8;
    pd->userid = pgpDigParamsUserID(pubp);
    if (! pd->userid) {
        pd->userid = "none";
    }
    pd->time = pgpDigParamsCreationTime(pubp);

    rasprintf(&pd->timestr, "%x", pd->time);
    rasprintf(&pd->verid, "%d:%s-%s", pgpDigParamsVersion(pubp), pd->fingerprint, pd->timestr);
}

static void finiPgpData(struct pgpdata_s *pd)
{
    free(pd->timestr);
    free(pd->verid);
    memset(pd, 0, sizeof(*pd));
}

static Header makeImmutable(Header h)
{
    h = headerReload(h, RPMTAG_HEADERIMMUTABLE);
    if (h != NULL) {
	char *sha1 = NULL;
	char *sha256 = NULL;
	unsigned int blen = 0;
	void *blob = headerExport(h, &blen);

	/* XXX FIXME: bah, this code is repeated in way too many places */
	rpmDigestBundle bundle = rpmDigestBundleNew();
	rpmDigestBundleAdd(bundle, RPM_HASH_SHA1, RPMDIGEST_NONE);
	rpmDigestBundleAdd(bundle, RPM_HASH_SHA256, RPMDIGEST_NONE);

	rpmDigestBundleUpdate(bundle, rpm_header_magic, sizeof(rpm_header_magic));
	rpmDigestBundleUpdate(bundle, blob, blen);

	rpmDigestBundleFinal(bundle, RPM_HASH_SHA1, (void **)&sha1, NULL, 1);
	rpmDigestBundleFinal(bundle, RPM_HASH_SHA256, (void **)&sha256, NULL, 1);

	if (sha1 && sha256) {
	    headerPutString(h, RPMTAG_SHA1HEADER, sha1);
	    headerPutString(h, RPMTAG_SHA256HEADER, sha256);
	} else {
	    h = headerFree(h);
	}
	free(sha1);
	free(sha256);
	free(blob);
	rpmDigestBundleFree(bundle);
    }
    return h;
}

/* Build pubkey header. */
static int makePubkeyHeader(rpmts ts, rpmPubkey key, Header * hdrp)
{
    Header h = headerNew();
    const char * afmt = "%{pubkeys:armor}";
    const char * group = "Public Keys";
    const char * license = "pubkey";
    const char * buildhost = "localhost";
    uint32_t zero = 0;
    struct pgpdata_s kd;
    char * d = NULL;
    char * enc = NULL;
    char * s = NULL;
    int rc = -1;

    memset(&kd, 0, sizeof(kd));

    if ((enc = rpmPubkeyBase64(key)) == NULL)
	goto exit;

    /* Build header elements. */
    initPgpData(key, &kd);

    rasprintf(&s, "%s public key", kd.userid);
    headerPutString(h, RPMTAG_PUBKEYS, enc);

    if ((d = headerFormat(h, afmt, NULL)) == NULL)
	goto exit;

    headerPutString(h, RPMTAG_NAME, "gpg-pubkey");
    headerPutString(h, RPMTAG_VERSION, kd.fingerprint);
    headerPutString(h, RPMTAG_RELEASE, kd.timestr);
    headerPutString(h, RPMTAG_DESCRIPTION, d);
    headerPutString(h, RPMTAG_GROUP, group);
    headerPutString(h, RPMTAG_LICENSE, license);
    headerPutString(h, RPMTAG_SUMMARY, s);
    headerPutString(h, RPMTAG_PACKAGER, kd.userid);
    headerPutUint32(h, RPMTAG_SIZE, &zero, 1);
    headerPutString(h, RPMTAG_RPMVERSION, RPMVERSION);
    headerPutString(h, RPMTAG_BUILDHOST, buildhost);
    headerPutUint32(h, RPMTAG_BUILDTIME, &kd.time, 1);
    headerPutString(h, RPMTAG_SOURCERPM, "(none)");

    addGpgProvide(h, kd.userid, kd.verid);
    addGpgProvide(h, kd.shortid, kd.verid);
    addGpgProvide(h, kd.signid, kd.verid);

    /* Reload it into immutable region and stomp standard digests on it */
    h = makeImmutable(h);
    if (h != NULL) {
	*hdrp = headerLink(h);
	rc = 0;

	/* Add install time/tid as icing on the cake */
	rpm_tid_t tid = rpmtsGetTid(ts);
	headerPutUint32(h, RPMTAG_INSTALLTIME, &tid, 1);
	headerPutUint32(h, RPMTAG_INSTALLTID, &tid, 1);
    }

exit:
    headerFree(h);
    finiPgpData(&kd);
    free(enc);
    free(d);
    free(s);

    return rc;
}
