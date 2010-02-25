#ifndef _RPMSCRIPT_H
#define _RPMSCRIPT_H

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>

typedef struct rpmScript_s * rpmScript;

struct rpmScript_s {
    rpmTag tag;		/* script tag */
    char **args;	/* scriptlet call arguments */
    char *body;		/* script body */
    char *descr;	/* description for logging */
};

RPM_GNUC_INTERNAL
rpmScript rpmScriptFromTag(Header h, rpmTag scriptTag);

RPM_GNUC_INTERNAL
rpmScript rpmScriptFree(rpmScript script);

RPM_GNUC_INTERNAL
rpmRC rpmScriptRun(rpmScript script, int arg1, int arg2,
                   rpmts ts, ARGV_const_t prefixes, int warn_only);

#endif /* _RPMSCRIPT_H */
