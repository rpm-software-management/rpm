
#include "system.h"

#include <rpm/rpmmacro.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>

#include "lib/rpmplugins.h"

#define STR1(x) #x
#define STR(x) STR1(x)

struct rpmPlugins_s {
    void **handles;
    ARGV_t names;
    int count;
    rpmts ts;
};

static int rpmpluginsGetPluginIndex(rpmPlugins plugins, const char *name)
{
    int i;
    for (i = 0; i < plugins->count; i++) {
	if (rstreq(plugins->names[i], name)) {
	    return i;
	}
    }
    return -1;
}

static int rpmpluginsHookIsSupported(void *handle, rpmPluginHook hook)
{
    rpmPluginHook *supportedHooks =
	(rpmPluginHook *) dlsym(handle, STR(PLUGIN_HOOKS));
    return (*supportedHooks & hook);
}

int rpmpluginsPluginAdded(rpmPlugins plugins, const char *name)
{
    return (rpmpluginsGetPluginIndex(plugins, name) >= 0);
}

rpmPlugins rpmpluginsNew(rpmts ts)
{
    rpmPlugins plugins = xcalloc(1, sizeof(*plugins));
    plugins->ts = ts;
    return plugins;
}

rpmRC rpmpluginsAdd(rpmPlugins plugins, const char *name, const char *path,
		    const char *opts)
{
    rpmPluginHook *supportedHooks;
    char *error;

    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
	rpmlog(RPMLOG_ERR, _("Failed to dlopen %s %s\n"), path, dlerror());
	return RPMRC_FAIL;
    }

    /* make sure the plugin has the supported hooks flag */
    supportedHooks = (rpmPluginHook *) dlsym(handle, STR(PLUGIN_HOOKS));
    if ((error = dlerror()) != NULL) {
	rpmlog(RPMLOG_ERR, _("Failed to resolve symbol %s: %s\n"),
	       STR(PLUGIN_HOOKS), error);
	return RPMRC_FAIL;
    }

    argvAdd(&plugins->names, name);
    plugins->handles = xrealloc(plugins->handles, (plugins->count + 1) * sizeof(*plugins->handles));
    plugins->handles[plugins->count] = handle;
    plugins->count++;

    return rpmpluginsCallInit(plugins, name, opts);
}

rpmRC rpmpluginsAddCollectionPlugin(rpmPlugins plugins, const char *name)
{
    char *path;
    char *options;
    rpmRC rc = RPMRC_FAIL;

    path = rpmExpand("%{?__collection_", name, "}", NULL);
    if (!path || rstreq(path, "")) {
	rpmlog(RPMLOG_ERR, _("Failed to expand %%__collection_%s macro\n"),
	       name);
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
	rpmpluginsCallCleanup(plugins, plugins->names[i]);
	dlclose(plugins->handles[i]);
    }
    plugins->handles = _free(plugins->handles);
    plugins->names = argvFree(plugins->names);
    plugins->ts = NULL;
    _free(plugins);

    return NULL;
}


/* Common define for all rpmpluginsCall* hook functions */
#define RPMPLUGINS_SET_HOOK_FUNC(hook) \
	void *handle = NULL; \
	int index; \
	char * error; \
	index = rpmpluginsGetPluginIndex(plugins, name); \
	if (index < 0) { \
		rpmlog(RPMLOG_ERR, _("Plugin %s not loaded\n"), name); \
		return RPMRC_FAIL; \
	} \
	handle = plugins->handles[index]; \
	if (!handle) { \
		rpmlog(RPMLOG_ERR, _("Plugin %s not loaded\n"), name); \
		return RPMRC_FAIL; \
	} \
	if (!rpmpluginsHookIsSupported(handle, hook)) { \
		return RPMRC_OK; \
	} \
	*(void **)(&hookFunc) = dlsym(handle, STR(hook##_FUNC)); \
	if ((error = dlerror()) != NULL) { \
		rpmlog(RPMLOG_ERR, _("Failed to resolve %s plugin symbol %s: %s\n"), name, STR(hook##_FUNC), error); \
		return RPMRC_FAIL; \
	} \
	if (rpmtsFlags(plugins->ts) & (RPMTRANS_FLAG_TEST | RPMTRANS_FLAG_JUSTDB)) { \
		return RPMRC_OK; \
	} \
	rpmlog(RPMLOG_DEBUG, "Plugin: calling hook %s in %s plugin\n", STR(hook##_FUNC), name);

rpmRC rpmpluginsCallInit(rpmPlugins plugins, const char *name, const char *opts)
{
    rpmRC (*hookFunc)(rpmts, const char *, const char *);
    RPMPLUGINS_SET_HOOK_FUNC(PLUGINHOOK_INIT);
    return hookFunc(plugins->ts, name, opts);
}

rpmRC rpmpluginsCallCleanup(rpmPlugins plugins, const char *name)
{
    rpmRC (*hookFunc)(void);
    RPMPLUGINS_SET_HOOK_FUNC(PLUGINHOOK_CLEANUP);
    return hookFunc();
}

rpmRC rpmpluginsCallOpenTE(rpmPlugins plugins, const char *name, rpmte te)
{
    rpmRC (*hookFunc)(rpmte);
    RPMPLUGINS_SET_HOOK_FUNC(PLUGINHOOK_OPENTE);
    return hookFunc(te);
}

rpmRC rpmpluginsCallCollectionPostAdd(rpmPlugins plugins, const char *name)
{
    rpmRC (*hookFunc)(void);
    RPMPLUGINS_SET_HOOK_FUNC(PLUGINHOOK_COLL_POST_ADD);
    return hookFunc();
}

rpmRC rpmpluginsCallCollectionPostAny(rpmPlugins plugins, const char *name)
{
    rpmRC (*hookFunc)(void);
    RPMPLUGINS_SET_HOOK_FUNC(PLUGINHOOK_COLL_POST_ANY);
    return hookFunc();
}

rpmRC rpmpluginsCallCollectionPreRemove(rpmPlugins plugins, const char *name)
{
    rpmRC (*hookFunc)(void);
    RPMPLUGINS_SET_HOOK_FUNC(PLUGINHOOK_COLL_PRE_REMOVE);
    return hookFunc();
}
