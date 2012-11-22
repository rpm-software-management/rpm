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

#define PLUGINHOOK_TSM_PRE_FUNC        pluginhook_tsm_pre
#define PLUGINHOOK_TSM_POST_FUNC        pluginhook_tsm_post

#define PLUGINHOOK_PSM_PRE_FUNC        pluginhook_psm_pre
#define PLUGINHOOK_PSM_POST_FUNC        pluginhook_psm_post

#define PLUGINHOOK_SCRIPTLET_PRE_FUNC    pluginhook_scriptlet_pre
#define PLUGINHOOK_SCRIPTLET_FORK_POST_FUNC    pluginhook_scriptlet_fork_post
#define PLUGINHOOK_SCRIPTLET_POST_FUNC    pluginhook_scriptlet_post

enum rpmPluginHook_e {
    PLUGINHOOK_NONE		= 0,
    PLUGINHOOK_INIT		= 1 << 0,
    PLUGINHOOK_CLEANUP		= 1 << 1,
    PLUGINHOOK_OPENTE		= 1 << 2,
    PLUGINHOOK_COLL_POST_ADD	= 1 << 3,
    PLUGINHOOK_COLL_POST_ANY	= 1 << 4,
    PLUGINHOOK_COLL_PRE_REMOVE	= 1 << 5,
    PLUGINHOOK_TSM_PRE         = 1 << 6,
    PLUGINHOOK_TSM_POST        = 1 << 7,
    PLUGINHOOK_PSM_PRE         = 1 << 8,
    PLUGINHOOK_PSM_POST        = 1 << 9,
    PLUGINHOOK_SCRIPTLET_PRE    = 1 << 10,
    PLUGINHOOK_SCRIPTLET_FORK_POST    = 1 << 11,
    PLUGINHOOK_SCRIPTLET_POST    = 1 << 12
};

/* indicates the way the scriptlet is executed */
typedef enum rpmScriptletExecutionFlow_e {
    RPMSCRIPTLET_NONE    = 0,
    RPMSCRIPTLET_FORK    = 1 << 0, 
    RPMSCRIPTLET_EXEC    = 1 << 1
} rpmScriptletExecutionFlow;

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
 * Add and open a rpm plugin
 * @param plugins	plugins structure to add a collection plugin to
 * @param type     type of plugin
 * @param name		name of plugin
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsAddPlugin(rpmPlugins plugins, const char *type, const char *name);

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

/** \ingroup rpmplugins
 * Call the pre transaction plugin hook
 * @param plugins	plugins structure
 * @param ts		processed transaction
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallTsmPre(rpmPlugins plugins, rpmts ts);

/** \ingroup rpmplugins
 * Call the post transaction plugin hook
 * @param plugins	plugins structure
 * @param ts		processed transaction
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallTsmPost(rpmPlugins plugins, rpmts ts);

/** \ingroup rpmplugins
 * Call the pre transaction element plugin hook
 * @param plugins	plugins structure
 * @param te		processed transaction element
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallPsmPre(rpmPlugins plugins, rpmte te);

/** \ingroup rpmplugins
 * Call the post transaction element plugin hook
 * @param plugins	plugins structure
 * @param te		processed transaction element
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallPsmPost(rpmPlugins plugins, rpmte te);

/** \ingroup rpmplugins
 * Call the pre scriptlet execution plugin hook
 * @param plugins	plugins structure
 * @param s_name	scriptlet name
 * @param type		indicates the scriptlet execution flow, see rpmScriptletExecutionFlow
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallScriptletPre(rpmPlugins plugins, const char *s_name, int type);

/** \ingroup rpmplugins
 * Call the post fork scriptlet plugin hook.
 * @param plugins	plugins structure
 * @param path		scriptlet path
 * @param type		indicates the scriptlet execution flow, see rpmScriptletExecutionFlow
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallScriptletForkPost(rpmPlugins plugins, const char *path, int type);

/** \ingroup rpmplugins
 * Call the post scriptlet execution plugin hook
 * @param plugins	plugins structure
 * @param s_name	scriptlet name
 * @param type		indicates the scriptlet execution flow, see rpmScriptletExecutionFlow
 * @param res		scriptlet execution result code
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallScriptletPost(rpmPlugins plugins, const char *s_name, int type, int res);

#ifdef __cplusplus
}
#endif
#endif	/* _PLUGINS_H */
