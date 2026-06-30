#ifndef _RPMSCRIPT_H
#define _RPMSCRIPT_H

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>
#include <rpm/rpmds.h>

/* Rpm scriptlet types */
enum rpmscriptTypes_e {
    RPMSCRIPT_PREIN		= (1 << 0),
    RPMSCRIPT_PREUN		= (1 << 1),
    RPMSCRIPT_POSTIN		= (1 << 2),
    RPMSCRIPT_POSTUN		= (1 << 3),
    RPMSCRIPT_TRIGGERPREIN	= (1 << 4),
    RPMSCRIPT_TRIGGERUN		= (1 << 6),
    RPMSCRIPT_TRIGGERIN		= (1 << 5),
    RPMSCRIPT_TRIGGERPOSTUN	= (1 << 7),
    RPMSCRIPT_PRETRANS		= (1 << 8),
    RPMSCRIPT_POSTTRANS		= (1 << 9),
    RPMSCRIPT_PREUNTRANS	= (1 << 10),
    RPMSCRIPT_POSTUNTRANS	= (1 << 11),
    RPMSCRIPT_SYSUSERS		= (1 << 12),
    /* ... */
    RPMSCRIPT_VERIFY		= (1 << 24),
};

typedef rpmFlags rpmscriptTypes;

enum rpmscriptTriggerMode_e {
    RPMSCRIPT_NORMALTRIGGER	= (1 << 0),
    RPMSCRIPT_FILETRIGGER	= (1 << 1),
    RPMSCRIPT_TRANSFILETRIGGER	= (1 << 2),
};

typedef rpmFlags rpmscriptTriggerModes;

enum rpmscriptFlags_e {
    RPMSCRIPT_FLAG_NONE		= 0,
    RPMSCRIPT_FLAG_EXPAND	= (1 << 0), /* macro expansion */
    RPMSCRIPT_FLAG_QFORMAT	= (1 << 1), /* header queryformat expansion */
    RPMSCRIPT_FLAG_CRITICAL	= (1 << 2), /* critical for success/failure */
};

typedef rpmFlags rpmscriptFlags;

typedef struct rpmScript_s * rpmScript;

typedef const char *(*nextfilefunc)(void *);

rpmTagVal triggerDsTag(rpmscriptTriggerModes tm);

rpmscriptTriggerModes triggerMode(rpmTagVal tag);

rpmTagVal triggertag(rpmsenseFlags sense);

rpmScript rpmScriptFromArgv(Header h, rpmTagVal scriptTag, ARGV_t argv, rpmscriptFlags flags, int chroot);

rpmScript rpmScriptFromTag(Header h, rpmTagVal scriptTag);

rpmScript rpmScriptFromTriggerTag(Header h, rpmTagVal triggerTag,
				    rpmscriptTriggerModes tm, uint32_t ix);

rpmScript rpmScriptFree(rpmScript script);

rpmRC rpmScriptRun(rpmScript script, int arg1, int arg2, FD_t scriptFd,
                   ARGV_const_t prefixes, rpmPlugins plugins);

rpmTagVal rpmScriptTag(rpmScript script);

rpmscriptTypes rpmScriptType(rpmScript script);

rpmscriptFlags rpmScriptFlags(rpmScript script);

void rpmScriptSetNextFileFunc(rpmScript script, nextfilefunc func,
			    void *param);

int rpmScriptChrootIn(rpmScript script);

int rpmScriptChrootOut(rpmScript script);

#endif /* _RPMSCRIPT_H */
