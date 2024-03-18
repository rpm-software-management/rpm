#include "system.h"

#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmplugin.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
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

    fd = open(state->fifo_path, O_WRONLY|O_NONBLOCK);
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

    state->fd = -1;
    return RPMRC_FAIL;
}

static void close_fifo(struct fapolicyd_data* state)
{
    if (state->fd > 0)
        (void) close(state->fd);

    state->fd = -1;
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

static void try_to_write_to_fifo(struct fapolicyd_data* state, const char * str)
{
    int reload = 0;
    int printed = 0;

    /* 1min/60s */
    const int timeout = 60;

    /* wait up to X seconds */
    for (int i = 0; i < timeout; i++) {

        if (reload) {
            if (!printed) {
                rpmlog(RPMLOG_WARNING, "rpm-plugin-fapolicyd: waiting for the service connection to resume, it can take up to %d seconds\n", timeout);
                printed = 1;
            }

            (void) close_fifo(state);
            (void) open_fifo(state);
        }

        if (state->fd >= 0) {
            if (write_fifo(state, str) == RPMRC_OK) {

                /* write was successful after few reopens */
                if (reload)
                    rpmlog(RPMLOG_WARNING, "rpm-plugin-fapolicyd: the service connection has resumed\n");

                break;
            }
        }

        /* failed write or reopen */
        reload = 1;
        sleep(1);

        /* the last iteration */
        /* consider failure */
        if (i == timeout-1) {
            rpmlog(RPMLOG_WARNING, "rpm-plugin-fapolicyd: the service connection has not resumed\n");
            rpmlog(RPMLOG_WARNING, "rpm-plugin-fapolicyd: continuing without the service\n");
        }

    }

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
    (void) close_fifo(&fapolicyd_state);
}

static rpmRC fapolicyd_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
        goto end;

    /* we are ready */
    if (fapolicyd_state.fd > 0) {
        /* send a signal that transaction is over */
        (void) try_to_write_to_fifo(&fapolicyd_state, "1\n");
        /* flush cache */
        (void) try_to_write_to_fifo(&fapolicyd_state, "2\n");
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
        (void) try_to_write_to_fifo(&fapolicyd_state, "2\n");

        /* optimize flushing */
        /* flush only when there was an actual change */
        fapolicyd_state.changed_files = 0;
    }

 end:
    return RPMRC_OK;
}

static rpmRC fapolicyd_fsm_file_prepare(rpmPlugin plugin, rpmfi fi,
                                        int fd, const char *path,
					const char *dest,
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

    snprintf(buffer, 4096, "%s %" PRIu64 " %64s\n", dest, size, sha);
    (void) try_to_write_to_fifo(&fapolicyd_state, buffer);

    free(sha);

 end:
    return RPMRC_OK;
}

struct rpmPluginHooks_s fapolicyd_hooks = {
    .init = fapolicyd_init,
    .cleanup = fapolicyd_cleanup,
    .tsm_post = fapolicyd_tsm_post,
    .scriptlet_pre = fapolicyd_scriptlet_pre,
    .fsm_file_prepare = fapolicyd_fsm_file_prepare,
};
