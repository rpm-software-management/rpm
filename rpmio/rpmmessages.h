#ifndef H_RPMMESSAGES
#define H_RPMMESSAGES

/** \ingroup rpmio
 * \file rpmio/rpmmessages.h
 */

/**
 */
typedef enum rpmmsgLevel_e {
    RPMMESS_DEBUG	= 1,	/*!< */
    RPMMESS_VERBOSE	= 2,	/*!< */
    RPMMESS_NORMAL	= 3,	/*!< */
    RPMMESS_WARNING	= 4,	/*!< */
    RPMMESS_ERROR	= 5,	/*!< */
    RPMMESS_FATALERROR	= 6 	/*!< */
} rpmmsgLevel;
#define	RPMMESS_QUIET (RPMMESS_NORMAL + 1)

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
typedef void * rpmCallbackData;

/**
 */
typedef void * (*rpmCallbackFunction)(const void * h, 
				      const rpmCallbackType what, 
				      const unsigned long amount, 
				      const unsigned long total,
				      const void * pkgKey,
				      rpmCallbackData data);

/**
 */
void urlSetCallback(rpmCallbackFunction notify, rpmCallbackData notifyData,
		int notifyCount);

/**
 */
void rpmIncreaseVerbosity(void);

/**
 */
void rpmSetVerbosity(int level);

/**
 */
int rpmGetVerbosity(void);

/**
 */
int rpmIsVerbose(void);

/**
 */
int rpmIsDebug(void);

/**
 */
#if defined(__GNUC__)
void rpmMessage(rpmmsgLevel level, const char * format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
#else
void rpmMessage(rpmmsgLevel level, const char * format, ...);
#endif


#ifdef __cplusplus
}
#endif

#endif  /* H_RPMMESSAGES */
