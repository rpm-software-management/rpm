#ifndef _RPMTYPES_H
#define _RPMTYPES_H

/** \ingroup rpmtypes
 * \file lib/rpmtypes.h
 *
 * Typedefs for RPM abstract data types.
 * @todo Add documentation...
 */

#include <rpm/rpmints.h>
#include <rpm/rpmtag.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const char *    errmsg_t;

typedef int32_t		rpm_tag_t;
typedef uint32_t	rpm_tagtype_t;
typedef uint32_t	rpm_count_t;

typedef void *		rpm_data_t;
typedef const void *	rpm_constdata_t;

typedef uint32_t	rpm_off_t;

typedef struct headerToken_s * Header;
typedef struct headerIterator_s * HeaderIterator;

typedef struct rpmts_s * rpmts;
typedef struct rpmte_s * rpmte;
typedef struct rpmds_s * rpmds;
typedef struct rpmfi_s * rpmfi;
typedef struct rpmdb_s * rpmdb;
typedef struct rpmdbMatchIterator_s * rpmdbMatchIterator;

typedef struct rpmal_s * rpmal;
typedef void * rpmalKey;

typedef struct rpmgi_s * rpmgi;

typedef struct rpmSpec_s * rpmSpec;

typedef const void * fnpyKey;
typedef void * rpmCallbackData;

typedef struct rpmRelocation_s rpmRelocation;

typedef struct _FD_s * FD_t;

/**
 * Package read return codes.
 */
typedef	enum rpmRC_e {
    RPMRC_OK		= 0,	/*!< Generic success code */
    RPMRC_NOTFOUND	= 1,	/*!< Generic not found code. */
    RPMRC_FAIL		= 2,	/*!< Generic failure code. */
    RPMRC_NOTTRUSTED	= 3,	/*!< Signature is OK, but key is not trusted. */
    RPMRC_NOKEY		= 4	/*!< Public key is unavailable. */
} rpmRC;

#ifdef __cplusplus
}
#endif

#endif /* _RPMTYPES_H */
