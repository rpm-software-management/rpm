#ifndef H_RPMMESSAGES
#define H_RPMMESSAGES

/** \ingroup rpmio
 * \file rpmio/rpmmessages.h
 * @todo Eliminate from API.
 */

#include "rpmlog.h"

#define	RPMMESS_DEBUG		RPMLOG_DEBUG
#define	RPMMESS_VERBOSE		RPMLOG_INFO
#define	RPMMESS_NORMAL		RPMLOG_NOTICE
#define	RPMMESS_WARNING		RPMLOG_WARNING
#define	RPMMESS_ERROR		RPMLOG_ERR
#define	RPMMESS_FATALERROR	RPMLOG_CRIT

#define	RPMMESS_QUIET		RPMMESS_WARNING

#define	rpmMessage		rpmlog
#define	rpmSetVerbosity(_lvl)	\
	((void)rpmlogSetMask( RPMLOG_UPTO( RPMLOG_PRI(_lvl))))
#define	rpmIncreaseVerbosity()	\
    ((void)rpmlogSetMask(((((unsigned)(rpmlogSetMask(0) & 0xff)) << 1) | 1)))
#define	rpmDecreaseVerbosity()	\
	((void)rpmlogSetMask((((int)(rpmlogSetMask(0) & 0xff)) >> 1)))
#define	rpmIsNormal()		\
	(rpmlogSetMask(0) >= RPMLOG_MASK( RPMMESS_NORMAL ))
#define	rpmIsVerbose()		\
	(rpmlogSetMask(0) >= RPMLOG_MASK( RPMMESS_VERBOSE ))
#define	rpmIsDebug()		\
	(rpmlogSetMask(0) >= RPMLOG_MASK( RPMMESS_DEBUG ))

/**
 */
typedef enum rpmCallbackType_e {
    RPMCALLBACK_INST_PROGRESS,
    RPMCALLBACK_INST_START,
    RPMCALLBACK_INST_OPEN_FILE,
    RPMCALLBACK_INST_CLOSE_FILE,
    RPMCALLBACK_TRANS_PROGRESS,
    RPMCALLBACK_TRANS_START,
    RPMCALLBACK_TRANS_STOP,
    RPMCALLBACK_UNINST_PROGRESS,
    RPMCALLBACK_UNINST_START,
    RPMCALLBACK_UNINST_STOP
} rpmCallbackType;

/**
 */
typedef void * rpmCallbackData;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
typedef /*@only@*/ /*@null@*/
    void * (*rpmCallbackFunction)
		(/*@null@*/ const void * h, 
		const rpmCallbackType what, 
		const unsigned long amount, 
		const unsigned long total,
		/*@null@*/ const void * pkgKey,
		/*@null@*/ rpmCallbackData data)
	/*@globals internalState@*/
	/*@modifies internalState@*/;

/**
 */
/*@unused@*/
void urlSetCallback(rpmCallbackFunction notify, rpmCallbackData notifyData,
		int notifyCount);

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMMESSAGES */
