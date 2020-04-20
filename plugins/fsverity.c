/**
 * Copyright (C) 2020 Facebook
 *
 * Author: Jes Sorensen <jsorensen@fb.com>
 */

#include "system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fsverity.h>

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

#include "sign/rpmsignverity.h"

static int sign_config_files = 0;

/*
 * This unconditionally tries to apply the fsverity signature to a file,
 * but fails gracefully if the file system doesn't support it or the
 * verity feature flag isn't enabled in the file system (ext4).
 */
static rpmRC fsverity_fsm_file_prepare(rpmPlugin plugin, rpmfi fi,
				       const char *path, const char *dest,
				       mode_t file_mode, rpmFsmOp op)
{
    struct fsverity_enable_arg arg;
    const unsigned char * signature = NULL;
    size_t len;
    uint16_t algo = 0;
    int rc = RPMRC_OK;
    int fd;
    rpmFileAction action = XFO_ACTION(op);
    char *buffer;

    /* Ignore skipped files and unowned directories */
    if (XFA_SKIPPING(action) || (op & FAF_UNOWNED)) {
	rpmlog(RPMLOG_DEBUG, "fsverity skipping early: path %s dest %s\n",
	       path, dest);
	goto exit;
    }

    /*
     * Do not install signatures for config files unless the
     * user explicitly asks for it.
     */
    if (rpmfiFFlags(fi) & RPMFILE_CONFIG) {
	if (!(rpmfiFMode(fi) & (S_IXUSR|S_IXGRP|S_IXOTH)) &&
	    !sign_config_files) {
	    rpmlog(RPMLOG_DEBUG, "fsverity skipping: path %s dest %s\n",
		   path, dest);

	    goto exit;
	}
    }

    /*
     * Right now fsverity doesn't deal with symlinks or directories, so do
     * not try to install signatures for non regular files.
     */
    if (!S_ISREG(rpmfiFMode(fi))) {
	rpmlog(RPMLOG_DEBUG, "fsverity skipping non regular: path %s dest %s\n",
	       path, dest);
	goto exit;
    }

    signature = rpmfiVSignature(fi, &len, &algo);
    if (!signature || !len) {
	rpmlog(RPMLOG_DEBUG, "fsverity no signature for: path %s dest %s\n",
	       path, dest);
	goto exit;
    }

    memset(&arg, 0, sizeof(arg));
    arg.version = 1;
    if (algo)
	arg.hash_algorithm = algo;
    else
	arg.hash_algorithm = FS_VERITY_HASH_ALG_SHA256;
    arg.block_size = RPM_FSVERITY_BLKSZ;
    arg.sig_ptr = (uintptr_t)signature;
    arg.sig_size = len;

    buffer = pgpHexStr(signature, arg.sig_size);
    rpmlog(RPMLOG_DEBUG, "applying signature: %s\n", buffer);
    free(buffer);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
	rpmlog(RPMLOG_ERR, "failed to open path %s\n", path);
	goto exit;
    }

    /*
     * Enable fsverity on the file.
     * fsverity not supported by file system (ENOTTY) and fsverity not
     * enabled on file system are expected and not considered
     * errors. Every other non-zero error code will result in the
     * installation failing.
     */
    if (ioctl(fd, FS_IOC_ENABLE_VERITY, &arg) != 0) {
	switch(errno) {
	case EBADMSG:
	    rpmlog(RPMLOG_DEBUG, "invalid or malformed fsverity signature for %s\n", path);
	    rc = RPMRC_FAIL;
	    break;
	case EEXIST:
	    rpmlog(RPMLOG_DEBUG, "fsverity signature already enabled %s\n",
		   path);
	    rc = RPMRC_FAIL;
	    break;
	case EINVAL:
	    rpmlog(RPMLOG_DEBUG, "invalid arguments for ioctl %s\n", path);
	    rc = RPMRC_FAIL;
	    break;
	case EKEYREJECTED:
	    rpmlog(RPMLOG_DEBUG, "signature doesn't match file %s\n", path);
	    rc = RPMRC_FAIL;
	    break;
	case EMSGSIZE:
	    rpmlog(RPMLOG_DEBUG, "invalid signature size for %s\n", path);
	    rc = RPMRC_FAIL;
	    break;
	case ENOPKG:
	    rpmlog(RPMLOG_DEBUG, "unsupported signature algoritm (%i) for %s\n",
		   arg.hash_algorithm, path);
	    rc = RPMRC_FAIL;
	    break;
	case ETXTBSY:
	    rpmlog(RPMLOG_DEBUG, "file is open by other process %s\n",
		   path);
	    rc = RPMRC_FAIL;
	    break;
	case ENOTTY:
	    rpmlog(RPMLOG_DEBUG, "fsverity not supported by file system for %s\n",
		   path);
	    break;
	case EOPNOTSUPP:
	    rpmlog(RPMLOG_DEBUG, "fsverity not enabled on file system for %s\n",
		   path);
	    break;
	default:
	    rpmlog(RPMLOG_DEBUG, "failed to enable verity (errno %i) for %s\n",
		   errno, path);
	    rc = RPMRC_FAIL;
	    break;
	}
    }

    rpmlog(RPMLOG_DEBUG, "fsverity enabled signature for: path %s dest %s\n",
	   path, dest);
    close(fd);
exit:
    return rc;
}

static rpmRC fsverity_init(rpmPlugin plugin, rpmts ts)
{
    sign_config_files = rpmExpandNumeric("%{?_fsverity_sign_config_files}");

    rpmlog(RPMLOG_DEBUG, "fsverity_init\n");

    return RPMRC_OK;
}

struct rpmPluginHooks_s fsverity_hooks = {
    .init = fsverity_init,
    .fsm_file_prepare = fsverity_fsm_file_prepare,
};
