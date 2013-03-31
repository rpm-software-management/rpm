
#include "system.h"

#include <rpm/rpmmacro.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>

#include "lib/rpmplugins.h"

#define STR1(x) #x
#define STR(x) STR1(x)

struct rpmPlugin_s {
    char *name;
    void *handle;
    rpmPluginHooks hooks;
};

struct rpmPlugins_s {
    rpmPlugin *plugins;
    int count;
    rpmts ts;
};

static rpmPlugin rpmpluginsGetPlugin(rpmPlugins plugins, const char *name)
{
    int i;
    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	if (rstreq(plugin->name, name)) {
	    return plugin;
	}
    }
    return NULL;
}

int rpmpluginsPluginAdded(rpmPlugins plugins, const char *name)
{
    return (rpmpluginsGetPlugin(plugins, name) != NULL);
}

rpmPlugins rpmpluginsNew(rpmts ts)
{
    rpmPlugins plugins = xcalloc(1, sizeof(*plugins));
    plugins->ts = ts;
    return plugins;
}

static rpmPlugin rpmPluginNew(const char *name, const char *path)
{
    rpmPlugin plugin = NULL;
    rpmPluginHooks hooks = NULL;
    char *error;
    char *hooks_name = NULL;

    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
	rpmlog(RPMLOG_ERR, _("Failed to dlopen %s %s\n"), path, dlerror());
	return NULL;
    }

    /* make sure the plugin has the supported hooks flag */
    hooks_name = rstrscat(NULL, name, "_hooks", NULL);
    hooks = dlsym(handle, hooks_name);
    if ((error = dlerror()) != NULL) {
	rpmlog(RPMLOG_ERR, _("Failed to resolve symbol %s: %s\n"),
	       hooks_name, error);
    } else {
	plugin = xcalloc(1, sizeof(*plugin));
	plugin->name = xstrdup(name);
	plugin->handle = handle;
	plugin->hooks = hooks;
    }
    free(hooks_name);

    return plugin;
}

static rpmPlugin rpmPluginFree(rpmPlugin plugin)
{
    if (plugin) {
	dlclose(plugin->handle);
	free(plugin->name);
	free(plugin);
    }
    return NULL;
}

rpmRC rpmpluginsAdd(rpmPlugins plugins, const char *name, const char *path,
		    const char *opts)
{
    rpmPlugin plugin = rpmPluginNew(name, path);;

    if (plugin == NULL)
	return RPMRC_FAIL;
    
    plugins->plugins = xrealloc(plugins->plugins,
			    (plugins->count + 1) * sizeof(*plugins->plugins));
    plugins->plugins[plugins->count] = plugin;
    plugins->count++;

    return rpmpluginsCallInit(plugins, name, opts);
}

rpmRC rpmpluginsAddPlugin(rpmPlugins plugins, const char *type, const char *name)
{
    char *path;
    char *options;
    rpmRC rc = RPMRC_FAIL;

    path = rpmExpand("%{?__", type, "_", name, "}", NULL);
    if (!path || rstreq(path, "")) {
	rpmlog(RPMLOG_ERR, _("Failed to expand %%__%s_%s macro\n"),
	       type, name);
	goto exit;
    }

    /* split the options from the path */
#define SKIPSPACE(s)    { while (*(s) &&  risspace(*(s))) (s)++; }
#define SKIPNONSPACE(s) { while (*(s) && !risspace(*(s))) (s)++; }
    options = path;
    SKIPNONSPACE(options);
    if (risspace(*options)) {
	*options = '\0';
	options++;
	SKIPSPACE(options);
    }
    if (*options == '\0') {
	options = NULL;
    }

    rc = rpmpluginsAdd(plugins, name, path, options);

  exit:
    _free(path);
    return rc;
}

rpmPlugins rpmpluginsFree(rpmPlugins plugins)
{
    int i;
    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	rpmpluginsCallCleanup(plugins, plugin->name);
	rpmPluginFree(plugin);
    }
    plugins->plugins = _free(plugins->plugins);
    plugins->ts = NULL;
    _free(plugins);

    return NULL;
}

#define RPMPLUGINS_GET_PLUGIN(name) \
	plugin = rpmpluginsGetPlugin(plugins, name); \
	if (plugin == NULL || plugin->handle == NULL) { \
		rpmlog(RPMLOG_ERR, _("Plugin %s not loaded\n"), name); \
		return RPMRC_FAIL; \
	}

/* Common define for all rpmpluginsCall* hook functions */
#define RPMPLUGINS_SET_HOOK_FUNC(hook) \
	rpmPluginHooks hooks = (plugin != NULL) ? plugin->hooks : NULL; \
	hookFunc = (hooks != NULL) ? hooks->hook : NULL; \
	if (hookFunc) { \
	    rpmlog(RPMLOG_DEBUG, "Plugin: calling hook %s in %s plugin\n", \
		   STR(hook), plugin->name); \
	}

rpmRC rpmpluginsCallInit(rpmPlugins plugins, const char *name, const char *opts)
{
    rpmRC rc = RPMRC_OK;
    plugin_init_func hookFunc;
    rpmPlugin plugin = NULL;
    RPMPLUGINS_GET_PLUGIN(name);
    RPMPLUGINS_SET_HOOK_FUNC(init);
    if (hookFunc)
	rc = hookFunc(plugins->ts, name, opts);
    return rc;
}

rpmRC rpmpluginsCallCleanup(rpmPlugins plugins, const char *name)
{
    rpmRC rc = RPMRC_OK;
    plugin_cleanup_func hookFunc;;
    rpmPlugin plugin = NULL;
    RPMPLUGINS_GET_PLUGIN(name);
    RPMPLUGINS_SET_HOOK_FUNC(cleanup);
    if (hookFunc)
	rc = hookFunc();
    return rc;
}

rpmRC rpmpluginsCallOpenTE(rpmPlugins plugins, const char *name, rpmte te)
{
    rpmRC rc = RPMRC_OK;
    plugin_opente_func hookFunc;
    rpmPlugin plugin = NULL;
    RPMPLUGINS_GET_PLUGIN(name);
    RPMPLUGINS_SET_HOOK_FUNC(opente);
    if (hookFunc)
	rc = hookFunc(te);
    return rc;
}

rpmRC rpmpluginsCallCollectionPostAdd(rpmPlugins plugins, const char *name)
{
    rpmRC rc = RPMRC_OK;
    plugin_coll_post_add_func hookFunc;
    rpmPlugin plugin = NULL;
    RPMPLUGINS_GET_PLUGIN(name);
    RPMPLUGINS_SET_HOOK_FUNC(coll_post_add);
    if (hookFunc)
	rc = hookFunc();
    return rc;
}

rpmRC rpmpluginsCallCollectionPostAny(rpmPlugins plugins, const char *name)
{
    rpmRC rc = RPMRC_OK;
    plugin_coll_post_any_func hookFunc;
    rpmPlugin plugin = NULL;
    RPMPLUGINS_GET_PLUGIN(name);
    RPMPLUGINS_SET_HOOK_FUNC(coll_post_any);
    if (hookFunc)
	rc = hookFunc();
    return rc;
}

rpmRC rpmpluginsCallCollectionPreRemove(rpmPlugins plugins, const char *name)
{
    rpmRC rc = RPMRC_OK;
    plugin_coll_pre_remove_func hookFunc;
    rpmPlugin plugin = NULL;
    RPMPLUGINS_GET_PLUGIN(name);
    RPMPLUGINS_SET_HOOK_FUNC(coll_pre_remove);
    if (hookFunc)
	rc = hookFunc();
    return rc;
}

rpmRC rpmpluginsCallTsmPre(rpmPlugins plugins, rpmts ts)
{
    plugin_tsm_pre_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(tsm_pre);
	if (hookFunc && hookFunc(ts) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallTsmPost(rpmPlugins plugins, rpmts ts, int res)
{
    plugin_tsm_post_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(tsm_post);
	if (hookFunc && hookFunc(ts, res) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallPsmPre(rpmPlugins plugins, rpmte te)
{
    plugin_psm_pre_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(psm_pre);
	if (hookFunc && hookFunc(te) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallPsmPost(rpmPlugins plugins, rpmte te, int res)
{
    plugin_psm_post_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(psm_post);
	if (hookFunc && hookFunc(te, res) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallScriptletPre(rpmPlugins plugins, const char *s_name, int type)
{
    plugin_scriptlet_pre_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(scriptlet_pre);
	if (hookFunc && hookFunc(s_name, type) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallScriptletForkPost(rpmPlugins plugins, const char *path, int type)
{
    plugin_scriptlet_fork_post_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(scriptlet_fork_post);
	if (hookFunc && hookFunc(path, type) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallScriptletPost(rpmPlugins plugins, const char *s_name, int type, int res)
{
    plugin_scriptlet_post_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(scriptlet_post);
	if (hookFunc && hookFunc(s_name, type, res) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallFsmFilePre(rpmPlugins plugins, const char* path,
                                mode_t file_mode, rpmFsmOp op)
{
    plugin_fsm_file_pre_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(fsm_file_pre);
	if (hookFunc && hookFunc(path, file_mode, op) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallFsmFilePost(rpmPlugins plugins, const char* path,
                                mode_t file_mode, rpmFsmOp op, int res)
{
    plugin_fsm_file_post_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(fsm_file_post);
	if (hookFunc && hookFunc(path, file_mode, op, res) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}

rpmRC rpmpluginsCallFsmFilePrepare(rpmPlugins plugins,
				   const char *path, const char *dest,
				   mode_t file_mode, rpmFsmOp op)
{
    plugin_fsm_file_prepare_func hookFunc;
    int i;
    rpmRC rc = RPMRC_OK;

    for (i = 0; i < plugins->count; i++) {
	rpmPlugin plugin = plugins->plugins[i];
	RPMPLUGINS_SET_HOOK_FUNC(fsm_file_prepare);
	if (hookFunc && hookFunc(path, dest, file_mode, op) == RPMRC_FAIL)
	    rc = RPMRC_FAIL;
    }

    return rc;
}
