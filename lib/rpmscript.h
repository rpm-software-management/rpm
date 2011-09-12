#ifndef _RPMSCRIPT_H
#define _RPMSCRIPT_H

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>

enum rpmscriptFlags_e {
    RPMSCRIPT_FLAG_NONE		= 0,
    RPMSCRIPT_FLAG_EXPAND	= (1 << 0), /* macro expansion */
    RPMSCRIPT_FLAG_QFORMAT	= (1 << 1), /* header queryformat expansion */
};

typedef rpmFlags rpmscriptFlags;

typedef struct rpmScript_s * rpmScript;

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
rpmScript rpmScriptFromTag(Header h, rpmTagVal scriptTag);

RPM_GNUC_INTERNAL
rpmScript rpmScriptFromTriggerTag(Header h, rpmTagVal triggerTag, uint32_t ix);

RPM_GNUC_INTERNAL
rpmScript rpmScriptFree(rpmScript script);

RPM_GNUC_INTERNAL
rpmRC rpmScriptRun(rpmScript script, int arg1, int arg2, FD_t scriptFd,
                   ARGV_const_t prefixes, int warn_only, int selinux);

RPM_GNUC_INTERNAL
rpmTagVal rpmScriptTag(rpmScript script);
#ifdef __cplusplus
}
#endif
#endif /* _RPMSCRIPT_H */
