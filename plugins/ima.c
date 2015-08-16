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
		if (fsig) {
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
