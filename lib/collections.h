#ifndef _COLLECTIONS_H
#define _COLLECTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef rpmRC(*collHookFunc) (rpmts, const char *, const char *);

#define COLLHOOK_POST_ADD_FUNC		post_add
#define COLLHOOK_POST_ANY_FUNC		post_any
#define COLLHOOK_PRE_REMOVE_FUNC	pre_remove

#define COLLECTION_HOOKS	collection_hooks
typedef enum rpmCollHook_e {
    COLLHOOK_NONE		= 0,
    COLLHOOK_POST_ADD		= 1 << 0,
    COLLHOOK_POST_ANY		= 1 << 1,
    COLLHOOK_PRE_REMOVE		= 1 << 2
} rpmCollHook;


#ifdef __cplusplus
}
#endif
#endif /* _COLLECTIONS_H */
