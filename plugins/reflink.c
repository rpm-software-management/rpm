#include "system.h"

#include <errno.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(__linux__)
#include <linux/fs.h>        /* For FICLONE */
#endif

#include <rpm/rpmlog.h>
#include "lib/rpmlib.h"
#include "lib/rpmplugin.h"
#include "lib/rpmte_internal.h"
#include <rpm/rpmfileutil.h>
#include "rpmio/rpmio_internal.h"


#include "debug.h"

#include <sys/ioctl.h>

/* use hash table to remember inode -> ix (for rpmfilesFN(ix)) lookups */
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE inodeIndexHash
#define HTKEYTYPE rpm_ino_t
#define HTDATATYPE int
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

/* We use this in find to indicate a key wasn't found. This is an
 * unrecoverable error, but we can at least show a decent error. 0 is never a
 * valid offset because it's the offset of the start of the file.
 */
#define NOT_FOUND 0

#define BUFFER_SIZE (1024 * 128)

/* magic value at end of file (64 bits) that indicates this is a transcoded
 * rpm.
 */
#define MAGIC 3472329499408095051

struct reflink_state_s {
    /* Stuff that's used across rpms */
    long fundamental_block_size;
    char *buffer;

    /* stuff that's used/updated per psm */
    uint32_t keys, keysize;

    /* table for current rpm, keys * (keysize + sizeof(rpm_loff_t)) */
    unsigned char *table;
    FD_t fd;
    rpmfiles files;
    inodeIndexHash inodeIndexes;
};

typedef struct reflink_state_s * reflink_state;

static int inodeCmp(rpm_ino_t a, rpm_ino_t b)
{
    return (a != b);
}

static unsigned int inodeId(rpm_ino_t a)
{
    /* rpm_ino_t is uint32_t so maps safely to unsigned int */
    return (unsigned int)a;
}

static rpmRC reflink_init(rpmPlugin plugin, rpmts ts) {
    reflink_state state = rcalloc(1, sizeof(struct reflink_state_s));

    /* IOCTL-FICLONERANGE(2): ...Disk filesystems generally require the offset
     * and length arguments to be aligned to the fundamental block size.
     *
     * The value of "fundamental block size" is directly related to the
     * system's page size, so we should use that.
     */
    state->fundamental_block_size = sysconf(_SC_PAGESIZE);
    state->buffer = rcalloc(1, BUFFER_SIZE);
    rpmPluginSetData(plugin, state);

    return RPMRC_OK;
}

static void reflink_cleanup(rpmPlugin plugin) {
    reflink_state state = rpmPluginGetData(plugin);
    free(state->buffer);
    free(state);
}

static rpmRC reflink_psm_pre(rpmPlugin plugin, rpmte te) {
    reflink_state state = rpmPluginGetData(plugin);
    state->fd = rpmteFd(te);
    if (state->fd == 0) {
	rpmlog(RPMLOG_DEBUG, _("reflink: fd = 0, no install\n"));
	return RPMRC_OK;
    }
    rpm_loff_t current = Ftell(state->fd);
    uint64_t magic;
    if (Fseek(state->fd, -(sizeof(magic)), SEEK_END) < 0) {
	rpmlog(RPMLOG_ERR, _("reflink: failed to seek for magic\n"));
	if (Fseek(state->fd, current, SEEK_SET) < 0) {
	    /* yes this gets a bit repetitive */
	    rpmlog(RPMLOG_ERR,
		 _("reflink: unable to seek back to original location\n"));
	}
	return RPMRC_FAIL;
    }
    size_t len = sizeof(magic);
    if (Fread(&magic, len, 1, state->fd) != len) {
	rpmlog(RPMLOG_ERR, _("reflink: unable to read magic\n"));
	if (Fseek(state->fd, current, SEEK_SET) < 0) {
	    rpmlog(RPMLOG_ERR,
		   _("reflink: unable to seek back to original location\n"));
	}
	return RPMRC_FAIL;
    }
    if (magic != MAGIC) {
	rpmlog(RPMLOG_DEBUG, _("reflink: not transcoded\n"));
	if (Fseek(state->fd, current, SEEK_SET) < 0) {
	    rpmlog(RPMLOG_ERR,
		   _("reflink: unable to seek back to original location\n"));
	    return RPMRC_FAIL;
	}
	return RPMRC_OK;
    }
    rpmlog(RPMLOG_DEBUG, _("reflink: *is* transcoded\n"));
    Header h = rpmteHeader(te);

    /* replace/add header that main fsm.c can read */
    headerDel(h, RPMTAG_PAYLOADFORMAT);
    headerPutString(h, RPMTAG_PAYLOADFORMAT, "clon");
    headerFree(h);
    state->files = rpmteFiles(te);
    /* tail of file contains offset_table, offset_checksums then magic */
    if (Fseek(state->fd, -(sizeof(rpm_loff_t) * 2 + sizeof(magic)), SEEK_END) < 0) {
	rpmlog(RPMLOG_ERR, _("reflink: failed to seek for tail %p\n"),
	       state->fd);
	return RPMRC_FAIL;
    }
    rpm_loff_t table_start;
    len = sizeof(table_start);
    if (Fread(&table_start, len, 1, state->fd) != len) {
	rpmlog(RPMLOG_ERR, _("reflink: unable to read table_start\n"));
	return RPMRC_FAIL;
    }
    if (Fseek(state->fd, table_start, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("reflink: unable to seek to table_start\n"));
	return RPMRC_FAIL;
    }
    len = sizeof(state->keys);
    if (Fread(&state->keys, len, 1, state->fd) != len) {
	rpmlog(RPMLOG_ERR, _("reflink: unable to read number of keys\n"));
	return RPMRC_FAIL;
    }
    len = sizeof(state->keysize);
    if (Fread(&state->keysize, len, 1, state->fd) != len) {
	rpmlog(RPMLOG_ERR, _("reflink: unable to read keysize\n"));
	return RPMRC_FAIL;
    }
    rpmlog(
	RPMLOG_DEBUG,
	_("reflink: table_start=0x%lx, keys=%d, keysize=%d\n"),
	table_start, state->keys, state->keysize
    );
    /* now get digest table if there is a reason to have one. */
    if (state->keys == 0 || state->keysize == 0) {
	/* no files (or no digests(!)) */
	state->table = NULL;
    } else {
	int table_size = state->keys * (state->keysize + sizeof(rpm_loff_t));
	state->table = rcalloc(1, table_size);
	if (Fread(state->table, table_size, 1, state->fd) != table_size) {
	    rpmlog(RPMLOG_ERR, _("reflink: unable to read table\n"));
	    return RPMRC_FAIL;
	}
	state->inodeIndexes = inodeIndexHashCreate(
	    state->keys, inodeId, inodeCmp, NULL, NULL
	);
    }

    /* Seek back to original location.
     * Might not be needed if we seek to offset immediately
     */
    if (Fseek(state->fd, current, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR,
	       _("reflink: unable to seek back to original location\n"));
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

static rpmRC reflink_psm_post(rpmPlugin plugin, rpmte te, int res)
{
    reflink_state state = rpmPluginGetData(plugin);
    state->files = rpmfilesFree(state->files);
    if (state->table) {
	free(state->table);
	state->table = NULL;
    }
    if (state->inodeIndexes) {
	inodeIndexHashFree(state->inodeIndexes);
	state->inodeIndexes = NULL;
    }
    return RPMRC_OK;
}


/* have a prototype, warnings system */
rpm_loff_t find(const unsigned char *digest, reflink_state state);

rpm_loff_t find(const unsigned char *digest, reflink_state state) {
# if defined(__GNUC__)
    /* GCC nested function because bsearch's comparison function can't access
     * state-keysize otherwise
     */
    int cmpdigest(const void *k1, const void *k2) {
	rpmlog(RPMLOG_DEBUG, _("reflink: cmpdigest k1=%p k2=%p\n"), k1, k2);
	return memcmp(k1, k2, state->keysize);
    }
# endif
    rpmlog(RPMLOG_DEBUG,
	   _("reflink: bsearch(key=%p, base=%p, nmemb=%d, size=%lu)\n"),
	   digest, state->table, state->keys,
	   state->keysize + sizeof(rpm_loff_t));
    char *entry = bsearch(digest, state->table, state->keys,
			  state->keysize + sizeof(rpm_loff_t), cmpdigest);
    if (entry == NULL) {
	return NOT_FOUND;
    }
    rpm_loff_t offset = *(rpm_loff_t *)(entry + state->keysize);
    return offset;
}

static rpmRC reflink_fsm_file_pre(rpmPlugin plugin, rpmfi fi, const char* path,
                                  mode_t file_mode, rpmFsmOp op)
{
    struct file_clone_range fcr;
    rpm_loff_t size;
    int dst, rc;
    int *hlix;

    reflink_state state = rpmPluginGetData(plugin);
    if (state->table == NULL) {
	/* no table means rpm is not in reflink format, so leave. Now. */
	return RPMRC_OK;
    }
    if (op == FA_TOUCH) {
	/* we're not overwriting an existing file. */
	return RPMRC_OK;
    }
    fcr.dest_offset = 0;
    if (S_ISREG(file_mode) && !(rpmfiFFlags(fi) & RPMFILE_GHOST)) {
	rpm_ino_t inode = rpmfiFInode(fi);
	/* check for hard link entry in table. GetEntry overwrites hlix with
	 * the address of the first match.
	 */
	if (inodeIndexHashGetEntry(state->inodeIndexes, inode, &hlix, NULL,
	                           NULL)) {
	    /* entry is in table, use hard link */
	    char *fn = rpmfilesFN(state->files, hlix[0]);
	    if (link(fn, path) != 0) {
		rpmlog(RPMLOG_ERR,
		       _("reflink: Unable to hard link %s -> %s due to %s\n"),
		       fn, path, strerror(errno));
		free(fn);
		return RPMRC_FAIL;
	    }
	    free(fn);
	    return RPMRC_PLUGIN_CONTENTS;
	}
	/* if we didn't hard link, then we'll track this inode as being
	 * created soon
	 */
	if (rpmfiFNlink(fi) > 1) {
	    /* minor optimization: only store files with more than one link */
	    inodeIndexHashAddEntry(state->inodeIndexes, inode, rpmfiFX(fi));
	}
	/* derived from wfd_open in fsm.c */
	mode_t old_umask = umask(0577);
	dst = open(path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR);
	umask(old_umask);
	if (dst == -1) {
	    rpmlog(RPMLOG_ERR,
		   _("reflink: Unable to open %s for writing due to %s, flags = %x\n"),
		   path, strerror(errno), rpmfiFFlags(fi));
	    return RPMRC_FAIL;
	}
	size = rpmfiFSize(fi);
	if (size > 0) {
	    /* round src_length down to fundamental_block_size multiple */
	    fcr.src_length = size / state->fundamental_block_size * state->fundamental_block_size;
	    if ((size % state->fundamental_block_size) > 0) {
		/* round up to next fundamental_block_size. We expect the data
		 * in the rpm to be similarly padded.
		 */
		fcr.src_length += state->fundamental_block_size;
	    }
	    fcr.src_fd = Fileno(state->fd);
	    if (fcr.src_fd == -1) {
		close(dst);
		rpmlog(RPMLOG_ERR, _("reflink: src fd lookup failed\n"));
		return RPMRC_FAIL;
	    }
	    fcr.src_offset = find(rpmfiFDigest(fi, NULL, NULL), state);
	    if (fcr.src_offset == NOT_FOUND) {
		close(dst);
		rpmlog(RPMLOG_ERR, _("reflink: digest not found\n"));
		return RPMRC_FAIL;
	    }
	    rpmlog(RPMLOG_DEBUG,
	           _("reflink: Reflinking %lu bytes at %lu to %s orig size=%lu, file=%ld\n"),
		   fcr.src_length, fcr.src_offset, path, size, fcr.src_fd);
	    rc = ioctl(dst, FICLONERANGE, &fcr);
	    if (rc) {
		rpmlog(RPMLOG_WARNING,
		       _("reflink: falling back to copying bits for %s due to %d, %d = %s\n"),
		       path, rc, errno, strerror(errno));
		if (Fseek(state->fd, fcr.src_offset, SEEK_SET) < 0) {
		    close(dst);
		    rpmlog(RPMLOG_ERR,
			   _("reflink: unable to seek on copying bits\n"));
		    return RPMRC_FAIL;
		}
		rpm_loff_t left = size;
		size_t len, read, written;
		while (left) {
		    len = (left > BUFFER_SIZE ? BUFFER_SIZE : left);
		    read = Fread(state->buffer, len, 1, state->fd);
		    if (read != len) {
			close(dst);
			rpmlog(RPMLOG_ERR,
			       _("reflink: short read on copying bits\n"));
			return RPMRC_FAIL;
		    }
		    written = write(dst, state->buffer, len);
		    if (read != written) {
			close(dst);
			rpmlog(RPMLOG_ERR,
			       _("reflink: short write on copying bits\n"));
			return RPMRC_FAIL;
		    }
		    left -= len;
		}
	    } else {
		/* reflink worked, so truncate */
		rc = ftruncate(dst, size);
		if (rc) {
		    rpmlog(RPMLOG_ERR,
			   _("reflink: Unable to truncate %s to %ld due to %s\n"),
			   path, size, strerror(errno));
		     return RPMRC_FAIL;
		}
	    }
	}
	close(dst);
	return RPMRC_PLUGIN_CONTENTS;
    }
    return RPMRC_OK;
}

struct rpmPluginHooks_s reflink_hooks = {
    .init = reflink_init,
    .cleanup = reflink_cleanup,
    .psm_pre = reflink_psm_pre,
    .psm_post = reflink_psm_post,
    .fsm_file_pre = reflink_fsm_file_pre,
};
