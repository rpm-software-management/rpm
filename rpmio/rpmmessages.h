#ifndef H_RPMMESSAGES
#define H_RPMMESSAGES

/** \ingroup rpmio
 * \file rpmio/rpmmessages.h
 */

#define	RPMMESS_DEBUG      1
#define	RPMMESS_VERBOSE    2
#define	RPMMESS_NORMAL     3
#define	RPMMESS_WARNING    4
#define	RPMMESS_ERROR      5
#define	RPMMESS_FATALERROR 6

#define	RPMMESS_QUIET (RPMMESS_NORMAL + 1)

typedef enum rpmCallbackType_e {
    RPMCALLBACK_INST_PROGRESS, RPMCALLBACK_INST_START,
    RPMCALLBACK_INST_OPEN_FILE, RPMCALLBACK_INST_CLOSE_FILE,
    RPMCALLBACK_TRANS_PROGRESS, RPMCALLBACK_TRANS_START, RPMCALLBACK_TRANS_STOP,
    RPMCALLBACK_UNINST_PROGRESS, RPMCALLBACK_UNINST_START, RPMCALLBACK_UNINST_STOP
} rpmCallbackType;

#ifdef __cplusplus
extern "C" {
#endif

typedef void * (*rpmCallbackFunction)(const void * h, 
				      const rpmCallbackType what, 
				      const unsigned long amount, 
				      const unsigned long total,
				      const void * pkgKey, void * data);

void	urlSetCallback(rpmCallbackFunction notify, void *notifyData, int notifyCount);

void rpmIncreaseVerbosity(void);
void rpmSetVerbosity(int level);
int rpmGetVerbosity(void);
int rpmIsVerbose(void);
int rpmIsDebug(void);
void rpmMessage(int level, const char * format, ...);

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMMESSAGES */
