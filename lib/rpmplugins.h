#ifndef _PLUGINS_H
#define _PLUGINS_H

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_HOOKS	plugin_hooks

#define PLUGINHOOK_INIT_FUNC			pluginhook_init
#define PLUGINHOOK_CLEANUP_FUNC			pluginhook_cleanup
#define PLUGINHOOK_OPENTE_FUNC			pluginhook_opente
#define PLUGINHOOK_COLL_POST_ADD_FUNC		pluginhook_coll_post_add
#define PLUGINHOOK_COLL_POST_ANY_FUNC		pluginhook_coll_post_any
#define PLUGINHOOK_COLL_PRE_REMOVE_FUNC		pluginhook_coll_pre_remove

enum rpmPluginHook_e {
    PLUGINHOOK_NONE		= 0,
    PLUGINHOOK_INIT		= 1 << 0,
    PLUGINHOOK_CLEANUP		= 1 << 1,
    PLUGINHOOK_OPENTE		= 1 << 2,
    PLUGINHOOK_COLL_POST_ADD	= 1 << 3,
    PLUGINHOOK_COLL_POST_ANY	= 1 << 4,
    PLUGINHOOK_COLL_PRE_REMOVE	= 1 << 5
};

typedef rpmFlags rpmPluginHook;

/** \ingroup rpmplugins
 * Create a new plugins structure
 * @param ts		transaction set
 * @return		new plugin structure
 */
rpmPlugins rpmpluginsNew(rpmts ts);

/** \ingroup rpmplugins
 * Destroy a plugins structure
 * @param plugins	plugins structure to destroy
 * @return		NULL always
 */
rpmPlugins rpmpluginsFree(rpmPlugins plugins);

/** \ingroup rpmplugins
 * Add and open a plugin
 * @param plugins	plugins structure to add a plugin to
 * @param name		name to access plugin
 * @param path		path of plugin to open
 * @param opts		options to pass to the plugin
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsAdd(rpmPlugins plugins, const char *name, const char *path, const char *opts);

/** \ingroup rpmplugins
 * Add and open a collection plugin
 * @param plugins	plugins structure to add a collection plugin to
 * @param name		name of collection to open
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsAddCollectionPlugin(rpmPlugins plugins, const char *name);

/** \ingroup rpmplugins
 * Determine if a plugin has been added already
 * @param plugins	plugins structure
 * @param name		name of plugin to check
 * @return		1 if plugin name has already been added, 0 otherwise
 */
int rpmpluginsPluginAdded(rpmPlugins plugins, const char *name);


/** \ingroup rpmplugins
 * Call the init plugin hook
 * @param plugins	plugins structure
 * @param name		name of plugin
 * @param opts		plugin options
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallInit(rpmPlugins plugins, const char *name, const char *opts);

/** \ingroup rpmplugins
 * Call the cleanup plugin hook
 * @param plugins	plugins structure
 * @param name		name of plugin
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallCleanup(rpmPlugins plugins, const char *name);

/** \ingroup rpmplugins
 * Call the open te plugin hook
 * @param plugins	plugins structure
 * @param name		name of plugin
 * @param te		transaction element opened
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallOpenTE(rpmPlugins plugins, const char *name, rpmte te);

/** \ingroup rpmplugins
 * Call the collection post add plugin hook
 * @param plugins	plugins structure
 * @param name		name of plugin
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallCollectionPostAdd(rpmPlugins plugins, const char *name);

/** \ingroup rpmplugins
 * Call the collection post any plugin hook
 * @param plugins	plugins structure
 * @param name		name of plugin
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallCollectionPostAny(rpmPlugins plugins, const char *name);

/** \ingroup rpmplugins
 * Call the collection pre remove plugin hook
 * @param plugins	plugins structure
 * @param name		name of plugin
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallCollectionPreRemove(rpmPlugins plugins, const char *name);

#ifdef __cplusplus
}
#endif
#endif	/* _PLUGINS_H */
