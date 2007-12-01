#ifndef H_RPMMESSAGES
#define H_RPMMESSAGES

/** \ingroup rpmio
 * \file rpmio/rpmmessages.h
 * @todo Eliminate from API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* LCL: ??? */
typedef const void * fnpyKey;

/**
 * Bit(s) to identify progress callbacks.
 */
typedef enum rpmCallbackType_e {
    RPMCALLBACK_UNKNOWN		= 0,
    RPMCALLBACK_INST_PROGRESS	= (1 <<  0),
    RPMCALLBACK_INST_START	= (1 <<  1),
    RPMCALLBACK_INST_OPEN_FILE	= (1 <<  2),
    RPMCALLBACK_INST_CLOSE_FILE	= (1 <<  3),
    RPMCALLBACK_TRANS_PROGRESS	= (1 <<  4),
    RPMCALLBACK_TRANS_START	= (1 <<  5),
    RPMCALLBACK_TRANS_STOP	= (1 <<  6),
    RPMCALLBACK_UNINST_PROGRESS	= (1 <<  7),
    RPMCALLBACK_UNINST_START	= (1 <<  8),
    RPMCALLBACK_UNINST_STOP	= (1 <<  9),
    RPMCALLBACK_REPACKAGE_PROGRESS = (1 << 10),
    RPMCALLBACK_REPACKAGE_START	= (1 << 11),
    RPMCALLBACK_REPACKAGE_STOP	= (1 << 12),
    RPMCALLBACK_UNPACK_ERROR	= (1 << 13),
    RPMCALLBACK_CPIO_ERROR	= (1 << 14)
} rpmCallbackType;

/**
 */
typedef void * rpmCallbackData;

/**
 */
typedef void * (*rpmCallbackFunction)
		(const void * h, 
		const rpmCallbackType what, 
		const unsigned long amount, 
		const unsigned long total,
		fnpyKey key,
		rpmCallbackData data);

#ifdef __cplusplus
}
#endif

#endif  /* H_RPMMESSAGES */
