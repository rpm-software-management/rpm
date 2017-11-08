#include "system.h"

#include <errno.h>
#include <sys/xattr.h>

#include <rpm/rpmfi.h>
#include <rpm/rpmte.h>
#include <rpm/rpmfiles.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmlog.h>
#include <rpmio/rpmstring.h>
#include <rpmio/rpmmacro.h>

#include "lib/rpmfs.h"
#include "lib/rpmplugin.h"
#include "lib/rpmte_internal.h"

#define XATTR_NAME_IMA "security.ima"

static int write_signatures_on_config_files = 0;

/*
 * check_zero_hdr: Check the signature for a zero header
 *
 * Check whether the given signature has a header with all zeros
 *
 * Returns -1 in case the signature is too short to compare
 * (invalid signature), 0 in case the header is not only zeroes,
 * and 1 if it is only zeroes.
 */
static int check_zero_hdr(const unsigned char *fsig, size_t siglen)
{
	/*
	 * Every signature has a header signature_v2_hdr as defined in
	 * Linux's (4.5) security/integrity/integtrity.h. The following
	 * 9 bytes represent this header in front of the signature.
	 */
	static const uint8_t zero_hdr[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (siglen < sizeof(zero_hdr))
		return -1;
	return (memcmp(fsig, &zero_hdr, sizeof(zero_hdr)) == 0);
}

static rpmRC ima_fsm_file_prepare(rpmPlugin plugin, rpmfi fi,
                                  const char *path,
                                  const char *dest,
                                  mode_t file_mode, rpmFsmOp op)
{
	const unsigned char * fsig = NULL;
	size_t len;
	int rc = RPMRC_OK;
	rpmFileAction action = XFO_ACTION(op);

	/* Ignore skipped files and unowned directories */
	if (XFA_SKIPPING(action) || (op & FAF_UNOWNED))
	    goto exit;

	/* Don't install signatures for (mutable) files marked
	 * as config files unless they are also executable or
	 * user specifically asks for it.
	 */
	if (rpmfiFFlags(fi) & RPMFILE_CONFIG) {
	    if (!(rpmfiFMode(fi) & (S_IXUSR|S_IXGRP|S_IXOTH)) &&
	        !write_signatures_on_config_files)
		goto exit;
	}

	fsig = rpmfiFSignature(fi, &len);
	if (fsig && (check_zero_hdr(fsig, len) == 0)) {
	    if (lsetxattr(path, XATTR_NAME_IMA, fsig, len, 0) < 0) {
	        rpmlog(RPMLOG_ERR,
			"ima: could not apply signature on '%s': %s\n",
			path, strerror(errno));
	        rc = RPMRC_FAIL;
	    }
	}

exit:
	return rc;
}

static rpmRC ima_init(rpmPlugin plugin, rpmts ts)
{
	write_signatures_on_config_files =
	    rpmExpandNumeric("%{?_ima_sign_config_files}");

	return RPMRC_OK;
}

struct rpmPluginHooks_s ima_hooks = {
        .init = ima_init,
	.fsm_file_prepare = ima_fsm_file_prepare,
};
