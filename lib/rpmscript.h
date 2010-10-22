#ifndef _RPMSCRIPT_H
#define _RPMSCRIPT_H

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>

enum rpmscriptFlags_e {
    RPMSCRIPT_NONE	= 0,
    RPMSCRIPT_EXPAND	= (1 << 0),     /* macro expansion */
    RPMSCRIPT_QFORMAT	= (1 << 1),	/* header queryformat expansion */
};

typedef rpmFlags rpmscriptFlags;

typedef struct rpmScript_s * rpmScript;

struct rpmScript_s {
    rpmTagVal tag;	/* script tag */
    char **args;	/* scriptlet call arguments */
    char *body;		/* script body */
    char *descr;	/* description for logging */
    rpmscriptFlags flags;	/* flags to control operation */
};

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
rpmScript rpmScriptFromTag(Header h, rpmTagVal scriptTag);

RPM_GNUC_INTERNAL
rpmScript rpmScriptFree(rpmScript script);

RPM_GNUC_INTERNAL
rpmRC rpmScriptRun(rpmScript script, int arg1, int arg2, FD_t scriptFd,
                   ARGV_const_t prefixes, int warn_only, int selinux);

#ifdef __cplusplus
}
#endif
#endif /* _RPMSCRIPT_H */
