#ifndef	H_RPMGI
#define	H_RPMGI

/** \ingroup rpmio
 * \file lib/rpmgi.h
 */

#include <rpmlib.h>
#include <rpmte.h>
#include <rpmts.h>
#include <argv.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
extern int _rpmgi_debug;

/**
 */
typedef enum rpmgiFlags_e {
    RPMGI_NONE		= 0,
    RPMGI_TSADD		= (1 << 0),
    RPMGI_TSORDER	= (1 << 1),
    RPMGI_NOGLOB	= (1 << 2),
    RPMGI_NOMANIFEST	= (1 << 3),
    RPMGI_NOHEADER	= (1 << 4)
} rpmgiFlags;

extern rpmgiFlags giFlags;

/**
 * Unreference a generalized iterator instance.
 * @param gi		generalized iterator
 * @param msg
 * @return		NULL always
 */
rpmgi rpmgiUnlink (rpmgi gi,
		const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmgi XrpmgiUnlink (rpmgi gi,
		const char * msg, const char * fn, unsigned ln);
#define	rpmgiUnlink(_gi, _msg)	XrpmgiUnlink(_gi, _msg, __FILE__, __LINE__)

/**
 * Reference a generalized iterator instance.
 * @param gi		generalized iterator
 * @param msg
 * @return		new generalized iterator reference
 */
rpmgi rpmgiLink (rpmgi gi, const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmgi XrpmgiLink (rpmgi gi, const char * msg,
		const char * fn, unsigned ln);
#define	rpmgiLink(_gi, _msg)	XrpmgiLink(_gi, _msg, __FILE__, __LINE__)

/** Destroy a generalized iterator.
 * @param gi		generalized iterator
 * @return		NULL always
 */
rpmgi rpmgiFree(rpmgi gi);

/**
 * Return a generalized iterator.
 * @param ts		transaction set
 * @param tag		rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		new iterator
 */
rpmgi rpmgiNew(rpmts ts, int tag, const void * keyp,
		size_t keylen);

/**
 * Perform next iteration step.
 * @param gi		generalized iterator
 * @returns		RPMRC_OK on success, RPMRC_NOTFOUND on EOI
 */
rpmRC rpmgiNext(rpmgi gi);

/**
 * Return current header path.
 * @param gi		generalized iterator
 * @returns		header path
 */
const char * rpmgiHdrPath(rpmgi gi);

/**
 * Return current iteration header.
 * @param gi		generalized iterator
 * @returns		header
 */
Header rpmgiHeader(rpmgi gi);

/**
 * Return current iteration transaction set.
 * @param gi		generalized iterator
 * @returns		transaction set
 */
rpmts rpmgiTs(rpmgi gi);

/**
 * Load iterator args.
 * @param gi		generalized iterator
 * @param argv		arg list
 * @param ftsOpts	fts(3) flags
 * @param flags		iterator flags
 * @returns		RPMRC_OK on success
 */
rpmRC rpmgiSetArgs(rpmgi gi, ARGV_t argv,
		int ftsOpts, rpmgiFlags flags);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGI */
