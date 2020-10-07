---
layout: default
title: rpm.org - RPM Plugin Interface (DRAFT)
---
# RPM Plugin Interface (DRAFT)

 The current plugin interface is concentrated around what goes on inside rpmtsRun(), and there's little that happens outside it. There's no reason the interface could not be enhanced to other directions, this is just a consequence of one of the primary motivations behind the plugins: enabling other projects like SELinux and MSSF to handle the area they know best, and without having to support a dozen different security "frameworks" within rpm itself.

The plugin API consists of a set of hooks that mostly come in pairs. All hooks receive the plugin itself as the first argument, similarly to this of C++ or self in Python. As of RPM 4.12.0 the following hooks exist:

```
struct rpmPluginHooks_s { 
    /* plugin constructor and destructor hooks */
    plugin_init_func                    init;
    plugin_cleanup_func                 cleanup;
    /* per transaction plugin hooks */
    plugin_tsm_pre_func                 tsm_pre;
    plugin_tsm_post_func                tsm_post;
    /* per transaction element hooks */
    plugin_psm_pre_func                 psm_pre;
    plugin_psm_post_func                psm_post;
    /* per scriptlet hooks */
    plugin_scriptlet_pre_func           scriptlet_pre;
    plugin_scriptlet_fork_post_func     scriptlet_fork_post;
    plugin_scriptlet_post_func          scriptlet_post;
    /* per file hooks */
    plugin_fsm_file_pre_func            fsm_file_pre;
    plugin_fsm_file_post_func           fsm_file_post;
    plugin_fsm_file_prepare_func        fsm_file_prepare;
};
```

## Initialization and cleanup

Hooks `init` and `cleanup` should be fairly obvious. Init occurs at the time of first transaction element addition, whether its for install, erase or reinstall.

Cleanup occurs on rpmtsFree() call.

## Per transaction hooks

Hooks `tsm_pre` and `tsm_post` occur before and after execution of a transaction. More precisely, pre runs before any scripts have been executed or files laid down, and post runs after all scripts have executed and files have been laid down.

Post hook is guaranteed to execute whenever pre hook was executed.

## Per transaction element hooks

Hooks `psm_pre` and `psm_post` occur before and after the processing of single transaction element within the transaction. Both hooks can get executed multiple times for a single element as these hooks get called separately for %pretrans and %posttrans scriptlets in addition to the main package install or erase action.

Post hook is guaranteed to execute whenever pre hook was executed.

## Per scriptlet hooks

Hooks `scriptlet_pre` and `scriptlet_post` occur just before and immediately after the execution of any rpm scriptlets, so these hooks can get called multiple times for a single element.

Post hook is guaranteed to execute whenever pre hook was executed.

The `scriptlet_fork_post` hook is special in since it occurs in the forked child just before exec() of the scriptlet interpreter, and in that it has no pairing pre-hook. The `scriptlet_fork_post` hook only ever executes if `scriptlet_pre` was successful.

## Per file hooks

Hooks `fsm_file_pre`, `fsm_file_post` and `fsm_file_prepare` execute for each individual file of all the transaction elements. Pre hook runs before a file (or directory) is created, post hook runs once rpm has done all its processing on the file. In between, `fsm_file_prepare` might be called. The prepare hook runs just after the file has been fully created and normal metadata such as ownership, permissions etc set, but before the file is moved into its final location. Note that prepare will sometimes be skipped, notably for hardlinked files where prepare gets called just once per hardlink set.

In most cases the fi argument is the handle to all the information on the file that rpm has, but it can be NULL in the case of unowned directories. The path argument might seem redundant for owned files but in `fsm_file_pre` and `fsm_file_prepare` it holds the temporary file name which is not accessible through fi

Post hook is is guaranteed to execute whenever pre hook was executed.

Warning: The exact relations and semantics of these hooks is subject to change as there are plans to improve rpms ability to undo file operations in case of failure.

## Examples

For a few simple examples, see the plugins shipped with rpm itself: [https://github.com/rpm-software-management/rpm/tree/master/plugins](https://github.com/rpm-software-management/rpm/tree/master/plugins)
