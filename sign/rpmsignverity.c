/**
 * Copyright (C) 2020 Facebook
 *
 * Author: Jes Sorensen <jsorensen@fb.com>
 */

#include "system.h"

#include <rpm/rpmlib.h>		/* RPMSIGTAG & related */
#include <rpm/rpmlog.h>		/* rpmlog */
#include <rpm/rpmfi.h>
#include <rpm/rpmpgp.h>		/* rpmDigestLength */
#include "lib/header.h"		/* HEADERGET_MINMEM */
#include "lib/header_internal.h"
#include "lib/rpmtypes.h"	/* rpmRC */
#include <libfsverity.h>
#include "rpmio/rpmio_internal.h"
#include "lib/rpmvs.h"

#include "sign/rpmsignverity.h"

#define MAX_SIGNATURE_LENGTH 1024

static int rpmVerityRead(void *opaque, void *buf, size_t size)
{
	int retval;
	rpmfi fi = (rpmfi)opaque;

	retval = rpmfiArchiveRead(fi, buf, size);

	if (retval > 0)
		retval = 0;
	return retval;
}

rpmRC rpmSignVerity(FD_t fd, Header sigh, Header h, char *key,
		    char *keypass, char *cert)
{
    int rc, status;
    FD_t gzdi;
    rpmfiles files = NULL;
    rpmfi fi = NULL;
    rpmts ts = rpmtsCreate();
    struct libfsverity_digest *digest = NULL;
    struct libfsverity_merkle_tree_params params;
    struct libfsverity_signature_params sig_params;
    rpm_loff_t file_size;
    off_t offset = Ftell(fd);
    const char *compr;
    char *rpmio_flags = NULL;
    char *digest_hex;
    uint8_t *sig;
    size_t sig_size;

    Fseek(fd, 0, SEEK_SET);
    rpmtsSetVSFlags(ts, RPMVSF_MASK_NODIGESTS | RPMVSF_MASK_NOSIGNATURES |
		    RPMVSF_NOHDRCHK);
    rc = rpmReadPackageFile(ts, fd, "fsverity", &h);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_DEBUG, _("%s: rpmReadPackageFile returned %i\n"),
	       __func__, rc);
	goto out;
    }

    rpmlog(RPMLOG_DEBUG, _("key: %s\n"), key);
    rpmlog(RPMLOG_DEBUG, _("cert: %s\n"), cert);

    compr = headerGetString(h, RPMTAG_PAYLOADCOMPRESSOR);
    rpmio_flags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);

    gzdi = Fdopen(fdDup(Fileno(fd)), rpmio_flags);
    free(rpmio_flags);

    files = rpmfilesNew(NULL, h, RPMTAG_BASENAMES, RPMFI_FLAGS_QUERY);
    fi = rpmfiNewArchiveReader(gzdi, files,
			       RPMFI_ITER_READ_ARCHIVE_OMIT_HARDLINKS);

    while (rpmfiNext(fi) >= 0) {
	if (!S_ISREG(rpmfiFMode(fi)))
	    continue;
	file_size = rpmfiFSize(fi);
	rpmlog(RPMLOG_DEBUG, _("file: %s, (size %li)\n"),
	       rpmfiFN(fi), file_size);

	memset(&params, 0, sizeof(struct libfsverity_merkle_tree_params));
	params.version = 1;
	params.hash_algorithm = FS_VERITY_HASH_ALG_SHA256;
	params.block_size = sysconf(_SC_PAGESIZE);
	params.salt_size = 0 /* salt_size */;
	params.salt = NULL /* salt */;
	params.file_size = file_size;
	status = libfsverity_compute_digest(fi, rpmVerityRead,
					    &params, &digest);
	if (!status) {
	    digest_hex = pgpHexStr(digest->digest, digest->digest_size);
	    rpmlog(RPMLOG_DEBUG, _("digest(%i): %s\n"),
		   digest->digest_size, digest_hex);
	    free(digest_hex);
	}
	memset(&sig_params, 0, sizeof(struct libfsverity_signature_params));
	sig_params.keyfile = key;
	sig_params.certfile = cert;
	if (libfsverity_sign_digest(digest, &sig_params, &sig, &sig_size)) {
	    rpmlog(RPMLOG_DEBUG, _("failed to sign digest\n"));
	    rc = RPMRC_FAIL;
	    goto out;
	}
	rpmlog(RPMLOG_DEBUG, _("digest signing success\n"));

	free(digest);
	free(sig);
    }

out:
    Fseek(fd, offset, SEEK_SET);

    rpmfilesFree(files);
    rpmfiFree(fi);
    rpmtsFree(ts);
    return rc;
}
