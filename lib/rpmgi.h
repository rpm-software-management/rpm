#ifndef	H_RPMGI
#define	H_RPMGI

/** \ingroup rpmio
 * \file lib/rpmgi.h
 */

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
extern int _rpmgi_debug;

/** \ingroup rpmgi
 */
typedef enum rpmgiFlags_e {
    RPMGI_NONE		= 0,
    RPMGI_NOGLOB	= (1 << 2),
    RPMGI_NOMANIFEST	= (1 << 3),
} rpmgiFlags;

extern rpmgiFlags giFlags;

/** \ingroup rpmgi 
 * Destroy a generalized iterator.
 * @param gi		generalized iterator
 * @return		NULL always
 */
rpmgi rpmgiFree(rpmgi gi);

/** \ingroup rpmgi
 * Return a generalized iterator.
 * @param ts		transaction set
 * @param tag		rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		new iterator
 */
rpmgi rpmgiNew(rpmts ts, rpmTag tag, const void * keyp,
		size_t keylen);

/** \ingroup rpmgi
 * Perform next iteration step.
 * @param gi		generalized iterator
 * @returns		RPMRC_OK on success, RPMRC_NOTFOUND on EOI
 */
rpmRC rpmgiNext(rpmgi gi);

/** \ingroup rpmgi
 * Return current iteration header.
 * @param gi		generalized iterator
 * @returns		header
 */
Header rpmgiHeader(rpmgi gi);

/** \ingroup rpmgi
 * Load iterator args.
 * @param gi		generalized iterator
 * @param argv		arg list
 * @param flags		iterator flags
 * @return 		RPMRC_OK on success
 */
rpmRC rpmgiSetArgs(rpmgi gi, ARGV_const_t argv, rpmgiFlags flags);


/** \ingroup rpmgi
 * Return number of errors (file not found etc) encountered during iteration
 * @param gi		generalized iterator
 * @return 		number of errors
 */
int rpmgiNumErrors(rpmgi gi);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGI */
