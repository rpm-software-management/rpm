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
 * @param flags		iterator flags
 * @param argv		arg list
 * @return		new iterator
 */
rpmgi rpmgiNew(rpmts ts, rpmgiFlags flags, ARGV_const_t argv);

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
 * Return number of errors (file not found etc) encountered during iteration
 * @param gi		generalized iterator
 * @return 		number of errors
 */
int rpmgiNumErrors(rpmgi gi);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGI */
