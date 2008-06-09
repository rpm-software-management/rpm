#ifndef _RPMCALLBACK_H
#define _RPMCALLBACK_H

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    RPMCALLBACK_REPACKAGE_PROGRESS = (1 << 10),	/* obsolete, unused */
    RPMCALLBACK_REPACKAGE_START	= (1 << 11),	/* obsolete, unused */
    RPMCALLBACK_REPACKAGE_STOP	= (1 << 12),	/* obsolete, unused */
    RPMCALLBACK_UNPACK_ERROR	= (1 << 13),
    RPMCALLBACK_CPIO_ERROR	= (1 << 14),
    RPMCALLBACK_SCRIPT_ERROR	= (1 << 15)
} rpmCallbackType;

/**
 */
typedef void * (*rpmCallbackFunction)
		(const void * h, 
		const rpmCallbackType what, 
		const rpm_loff_t amount, 
		const rpm_loff_t total,
		fnpyKey key,
		rpmCallbackData data);

#ifdef __cplusplus
}
#endif

#endif /* _RPMCALLBACK_H */
