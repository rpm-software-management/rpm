#ifndef _PLUGINS_H
#define _PLUGINS_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmfi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* indicates the way the scriptlet is executed */
typedef enum rpmScriptletExecutionFlow_e {
    RPMSCRIPTLET_NONE    = 0,
    RPMSCRIPTLET_FORK    = 1 << 0, 
    RPMSCRIPTLET_EXEC    = 1 << 1
} rpmScriptletExecutionFlow;


/** \ingroup rpmfi
 * File disposition flags during package install/erase transaction.
 * XXX: Move these to rpmfi.h once things stabilize.
 */
enum rpmFileActionFlags_e {
    /* bits 0-15 reserved for actions */
    FAF_UNOWNED		= (1 << 31)
};
typedef rpmFlags rpmFileActionFlags;

/** \ingroup rpmfi
 * File action and associated flags on install/erase
 */
typedef rpmFlags rpmFsmOp;

#define XFA_MASK	0x0000ffff
#define XFAF_MASK	~(XFA_MASK)
#define XFO_ACTION(_a)	((_a) & XFA_MASK)	/*!< File op action part */
#define XFO_FLAGS(_a)	((_a) & XFAF_MASK)	/*!< File op flags part */

/* plugin hook typedefs */
typedef rpmRC (*plugin_init_func)(rpmts ts,
				 const char * name, const char * opts);
typedef rpmRC (*plugin_cleanup_func)(void);
typedef rpmRC (*plugin_opente_func)(rpmte te);
typedef rpmRC (*plugin_coll_post_any_func)(void);
typedef rpmRC (*plugin_coll_post_add_func)(void);
typedef rpmRC (*plugin_coll_pre_remove_func)(void);
typedef rpmRC (*plugin_tsm_pre_func)(rpmts ts);
typedef rpmRC (*plugin_tsm_post_func)(rpmts ts, int res);
typedef rpmRC (*plugin_psm_pre_func)(rpmte te);
typedef rpmRC (*plugin_psm_post_func)(rpmte te, int res);
typedef rpmRC (*plugin_scriptlet_pre_func)(const char *s_name, int type);
typedef rpmRC (*plugin_scriptlet_fork_post_func)(const char *path, int type);
typedef rpmRC (*plugin_scriptlet_post_func)(const char *s_name, int type,
					    int res);
typedef rpmRC (*plugin_fsm_file_pre_func)(const char* path, mode_t file_mode,
					  rpmFsmOp op);
typedef rpmRC (*plugin_fsm_file_post_func)(const char* path, mode_t file_mode,
					  rpmFsmOp op, int res);
typedef rpmRC (*plugin_fsm_file_prepare_func)(const char* path,
					      const char *dest,
					      mode_t file_mode, rpmFsmOp op);

typedef struct rpmPluginHooks_s * rpmPluginHooks;
struct rpmPluginHooks_s {
    /* plugin constructor and destructor hooks */
    plugin_init_func			init;
    plugin_cleanup_func			cleanup;
    /* collection plugin hooks */
    plugin_opente_func			opente;
    plugin_coll_post_any_func		coll_post_any;
    plugin_coll_post_add_func		coll_post_add;
    plugin_coll_pre_remove_func		coll_pre_remove;
    /* per transaction plugin hooks */
    plugin_tsm_pre_func			tsm_pre;
    plugin_tsm_post_func		tsm_post;
    /* per transaction element hooks */
    plugin_psm_pre_func			psm_pre;
    plugin_psm_post_func		psm_post;
    /* per scriptlet hooks */
    plugin_scriptlet_pre_func		scriptlet_pre;
    plugin_scriptlet_fork_post_func	scriptlet_fork_post;
    plugin_scriptlet_post_func		scriptlet_post;
    /* per file hooks */
    plugin_fsm_file_pre_func		fsm_file_pre;
    plugin_fsm_file_post_func		fsm_file_post;
    plugin_fsm_file_prepare_func	fsm_file_prepare;
};

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
 * @param res		transaction result code
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallTsmPost(rpmPlugins plugins, rpmts ts, int res);

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
 * @param res		transaction element result code
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallPsmPost(rpmPlugins plugins, rpmte te, int res);

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

/** \ingroup rpmplugins
 * Call the fsm file pre plugin hook
 * @param plugins	plugins structure
 * @param path		file object path
 * @param file_mode	file object mode
 * @param op		file operation + associated flags
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallFsmFilePre(rpmPlugins plugins, const char* path,
                                mode_t file_mode, rpmFsmOp op);

/** \ingroup rpmplugins
 * Call the fsm file post plugin hook
 * @param plugins	plugins structure
 * @param path		file object path
 * @param file_mode	file object mode
 * @param op		file operation + associated flags
 * @param res		fsm result code
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallFsmFilePost(rpmPlugins plugins, const char* path,
                                mode_t file_mode, rpmFsmOp op, int res);

/** \ingroup rpmplugins
 * Call the fsm file prepare plugin hook. Called after setting
 * permissions etc, but before committing file to destination path.
 * @param plugins	plugins structure
 * @param path		file object current path
 * @param path		file object destination path
 * @param file_mode	file object mode
 * @param op		file operation + associated flags
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmpluginsCallFsmFilePrepare(rpmPlugins plugins,
                                   const char *path, const char *dest,
                                   mode_t mode, rpmFsmOp op);

#ifdef __cplusplus
}
#endif
#endif	/* _PLUGINS_H */
