#include "system.h"

#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include "lib/rpmplugin.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

struct fapolicyd_data {
    int fd;
    long changed_files;
    const char * fifo_path;
};

static struct fapolicyd_data fapolicyd_state = {
    .fd = -1,
    .changed_files = 0,
    .fifo_path = "/run/fapolicyd/fapolicyd.fifo",
};

static rpmRC open_fifo(struct fapolicyd_data* state)
{
    int fd = -1;
    struct stat s;

    fd = open(state->fifo_path, O_RDWR);
    if (fd == -1) {
        rpmlog(RPMLOG_DEBUG, "Open: %s -> %s\n", state->fifo_path, strerror(errno));
        goto bad;
    }

    if (stat(state->fifo_path, &s) == -1) {
        rpmlog(RPMLOG_DEBUG, "Stat: %s -> %s\n", state->fifo_path, strerror(errno));
        goto bad;
    }

    if (!S_ISFIFO(s.st_mode)) {
        rpmlog(RPMLOG_DEBUG, "File: %s exists but it is not a pipe!\n", state->fifo_path);
        goto bad;
    }

    /* keep only file's permition bits */
    mode_t mode = s.st_mode & ~S_IFMT;

    /* we require pipe to have 0660 permission */
    if (mode != 0660) {
        rpmlog(RPMLOG_ERR, "File: %s has %o instead of 0660 \n",
               state->fifo_path,
               mode );
        goto bad;
    }

    state->fd = fd;
    /* considering success */
    return RPMRC_OK;

 bad:
    if (fd >= 0)
        close(fd);
    return RPMRC_FAIL;
}

static rpmRC write_fifo(struct fapolicyd_data* state, const char * str)
{
    ssize_t len = strlen(str);
    ssize_t written = 0;
    ssize_t n = 0;

    while (written < len) {
        if ((n = write(state->fd, str + written, len - written)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            rpmlog(RPMLOG_DEBUG, "Write: %s -> %s\n", state->fifo_path, strerror(errno));
            goto bad;
        }
        written += n;
    }

    return RPMRC_OK;

 bad:
    return RPMRC_FAIL;
}

static rpmRC fapolicyd_init(rpmPlugin plugin, rpmts ts)
{
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
        goto end;

    if (!rstreq(rpmtsRootDir(ts), "/"))
        goto end;

    (void) open_fifo(&fapolicyd_state);

 end:
    return RPMRC_OK;
}

static void fapolicyd_cleanup(rpmPlugin plugin)
{
    if (fapolicyd_state.fd > 0)
        (void) close(fapolicyd_state.fd);

    fapolicyd_state.fd = -1;
}

static rpmRC fapolicyd_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
        goto end;

    /* we are ready */
    if (fapolicyd_state.fd > 0) {
        /* send a signal that transaction is over */
        (void) write_fifo(&fapolicyd_state, "1\n");
        /* flush cache */
        (void) write_fifo(&fapolicyd_state, "2\n");
    }

 end:
    return RPMRC_OK;
}

static rpmRC fapolicyd_scriptlet_pre(rpmPlugin plugin, const char *s_name,
                                     int type)
{
    if (fapolicyd_state.fd == -1)
        goto end;

    if (fapolicyd_state.changed_files > 0) {
        /* send signal to flush cache */
        (void) write_fifo(&fapolicyd_state, "2\n");

        /* optimize flushing */
        /* flush only when there was an actual change */
        fapolicyd_state.changed_files = 0;
    }

 end:
    return RPMRC_OK;
}

static rpmRC fapolicyd_fsm_file_prepare(rpmPlugin plugin, rpmfi fi,
                                        const char *path, const char *dest,
                                        mode_t file_mode, rpmFsmOp op)
{
    /* not ready  */
    if (fapolicyd_state.fd == -1)
        goto end;

    rpmFileAction action = XFO_ACTION(op);

    /* Ignore skipped files and unowned directories */
    if (XFA_SKIPPING(action) || (op & FAF_UNOWNED)) {
        rpmlog(RPMLOG_DEBUG, "fapolicyd skipping early: path %s dest %s\n",
               path, dest);
        goto end;
    }

    if (!S_ISREG(rpmfiFMode(fi))) {
        rpmlog(RPMLOG_DEBUG, "fapolicyd skipping non regular: path %s dest %s\n",
               path, dest);
        goto end;
    }

    fapolicyd_state.changed_files++;

    char buffer[4096];

    rpm_loff_t size = rpmfiFSize(fi);
    char * sha = rpmfiFDigestHex(fi, NULL);

    snprintf(buffer, 4096, "%s %lu %64s\n", dest, size, sha);
    (void) write_fifo(&fapolicyd_state, buffer);

    free(sha);

 end:
    return RPMRC_OK;
}

struct rpmPluginHooks_s fapolicyd_hooks = {
    .init = fapolicyd_init,
    .cleanup = fapolicyd_cleanup,
    .scriptlet_pre = fapolicyd_scriptlet_pre,
    .tsm_post = fapolicyd_tsm_post,
    .fsm_file_prepare = fapolicyd_fsm_file_prepare,
};
