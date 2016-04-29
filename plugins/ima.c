#include <sys/xattr.h>

#include <rpm/rpmfi.h>
#include <rpm/rpmte.h>
#include <rpm/rpmfiles.h>
#include <rpm/rpmtypes.h>
#include <rpmio/rpmstring.h>

#include "lib/rpmfs.h"
#include "lib/rpmplugin.h"
#include "lib/rpmte_internal.h"

#define XATTR_NAME_IMA "security.ima"

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

static rpmRC ima_psm_post(rpmPlugin plugin, rpmte te, int res)
{
	rpmfi fi = rpmteFI(te);
	const char *fpath;
	const unsigned char * fsig = NULL;
	size_t len;
	int rc = 0;

	if (fi == NULL) {
	    rc = RPMERR_BAD_MAGIC;
	    goto exit;
	}

	while (rpmfiNext(fi) >= 0) {
	    /* Don't install signatures for (mutable) config files */
	    if (!(rpmfiFFlags(fi) & RPMFILE_CONFIG)) {
		fpath = rpmfiFN(fi);
		fsig = rpmfiFSignature(fi, &len);
		if (fsig && (check_zero_hdr(fsig, len) == 0)) {
		    lsetxattr(fpath, XATTR_NAME_IMA, fsig, len, 0);
		}
	    }
	}
exit:
	return rc;
}

struct rpmPluginHooks_s ima_hooks = {
	.psm_post = ima_psm_post,
};
