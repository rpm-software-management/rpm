#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpm/rpmplugin.h>
#include <rpm/rpmte.h>

static rpmRC debug_init(rpmPlugin plugin, rpmts ts)
{
        fprintf(stderr, "%s\n", __func__);
        return RPMRC_OK;
}

static void debug_cleanup(rpmPlugin plugin)
{
        fprintf(stderr, "%s\n", __func__);
}

static rpmRC debug_tsm_pre(rpmPlugin plugin, rpmts ts)
{
        fprintf(stderr, "%s\n", __func__);
        return RPMRC_OK;
}

static rpmRC debug_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
        fprintf(stderr, "%s: %d\n", __func__, res);
        return RPMRC_OK;
}

static rpmRC debug_psm_pre(rpmPlugin plugin, rpmte te)
{
        fprintf(stderr, "%s: %s\n", __func__, rpmteNEVRA(te));
        return RPMRC_OK;
}

static rpmRC debug_psm_post(rpmPlugin plugin, rpmte te, int res)
{
        fprintf(stderr, "%s: %s:%d\n", __func__, rpmteNEVRA(te), res);
        return RPMRC_OK;
}

static rpmRC debug_scriptlet_pre(rpmPlugin plugin,
				const char *s_name, int type)
{
        fprintf(stderr, "%s: %s %d\n", __func__, s_name, type);
        return RPMRC_OK;
}

static rpmRC debug_scriptlet_fork_post(rpmPlugin plugin,
				    const char *path, int type)
{
        fprintf(stderr, "%s: %s %d\n", __func__, path, type);
        return RPMRC_OK;
}

static rpmRC debug_scriptlet_post(rpmPlugin plugin,
				    const char *s_name, int type, int res)
{
        fprintf(stderr, "%s: %s %d: %d\n", __func__, s_name, type, res);
        return RPMRC_OK;
}

/* we can't predict the transaction id so hide it from the output */
static char *cleanpath(const char *path)
{
    char *p = rstrdup(path);
    char *t = strrchr(p, ';');
    if (t)
	*(t+1) = '\0';
    return p;
}

static rpmRC debug_fsm_file_pre(rpmPlugin plugin, rpmfi fi,
		const char* path, mode_t file_mode, rpmFsmOp op)
{
    char *p = cleanpath(path);
    fprintf(stderr, "%s: %s %s %o %x\n", __func__,
		rpmfiFN(fi), p, file_mode, op);
    free(p);
    return RPMRC_OK;
}
static rpmRC debug_fsm_file_post(rpmPlugin plugin, rpmfi fi,
		const char* path, mode_t file_mode, rpmFsmOp op, int res)
{
    char *p = cleanpath(path);
    fprintf(stderr, "%s: %s %s %o %x: %d\n", __func__,
		rpmfiFN(fi), p, file_mode, op, res);
    free(p);
    return RPMRC_OK;
}
static rpmRC debug_fsm_file_prepare(rpmPlugin plugin, rpmfi fi,
		int fd, const char* path, const char *dest,
		mode_t file_mode, rpmFsmOp op)
{
    char *p = cleanpath(path);
    fprintf(stderr, "%s: %s %d %s %s %o %x\n", __func__,
		rpmfiFN(fi), (fd != -1), p, dest, file_mode, op);
    free(p);
    return RPMRC_OK;
}

struct rpmPluginHooks_s debug_hooks = {
        .init = debug_init,
        .cleanup = debug_cleanup,
        .tsm_pre = debug_tsm_pre,
        .tsm_post = debug_tsm_post,
        .psm_pre = debug_psm_pre,
        .psm_post = debug_psm_post,
        .scriptlet_pre = debug_scriptlet_pre,
        .scriptlet_fork_post = debug_scriptlet_fork_post,
        .scriptlet_post = debug_scriptlet_post,
        .fsm_file_pre = debug_fsm_file_pre,
        .fsm_file_post = debug_fsm_file_post,
        .fsm_file_prepare = debug_fsm_file_prepare,
};
