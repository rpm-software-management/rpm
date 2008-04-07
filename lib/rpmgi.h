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
    RPMGI_TSADD		= (1 << 0),
    RPMGI_TSORDER	= (1 << 1),
    RPMGI_NOGLOB	= (1 << 2),
    RPMGI_NOMANIFEST	= (1 << 3),
    RPMGI_NOHEADER	= (1 << 4)
} rpmgiFlags;

/** \ingroup rpmgi
 * rpmgi FTS-flags
 */
#define RPMGI_COMFOLLOW   0x0001          /* follow command line symlinks */
#define RPMGI_LOGICAL     0x0002          /* logical walk */
#define RPMGI_NOCHDIR     0x0004          /* don't change directories */
#define RPMGI_NOSTAT      0x0008          /* don't get stat info */
#define RPMGI_PHYSICAL    0x0010          /* physical walk */
#define RPMGI_SEEDOT      0x0020          /* return dot and dot-dot */
#define RPMGI_XDEV        0x0040          /* don't cross devices */
#define RPMGI_WHITEOUT    0x0080          /* return whiteout information */

extern rpmgiFlags giFlags;

/** \ingroup rpmgi
 * Unreference a generalized iterator instance.
 * @param gi		generalized iterator
 * @param msg
 * @return		NULL always
 */
rpmgi rpmgiUnlink (rpmgi gi, const char * msg);

/** \ingroup rpmgi
 * Reference a generalized iterator instance.
 * @param gi		generalized iterator
 * @param msg
 * @return		new generalized iterator reference
 */
rpmgi rpmgiLink (rpmgi gi, const char * msg);

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
 * Return current header path.
 * @param gi		generalized iterator
 * @returns		header path
 */
const char * rpmgiHdrPath(rpmgi gi);

/** \ingroup rpmgi
 * Return current iteration header.
 * @param gi		generalized iterator
 * @returns		header
 */
Header rpmgiHeader(rpmgi gi);

/** \ingroup rpmgi
 * Return current iteration transaction set.
 * @param gi		generalized iterator
 * @returns		transaction set
 */
rpmts rpmgiTs(rpmgi gi);

/** \ingroup rpmgi
 * Load iterator args.
 * @param gi		generalized iterator
 * @param argv		arg list
 * @param ftsOpts	fts(3) flags
 * @param flags		iterator flags
 * @return 		RPMRC_OK on success
 */
rpmRC rpmgiSetArgs(rpmgi gi, ARGV_const_t argv,
		int ftsOpts, rpmgiFlags flags);


/** \ingroup rpmgi
 * Retrieve iterator flags
 * @param gi		generalized iterator
 * @return		iterator flags
 */
rpmgiFlags rpmgiGetFlags(rpmgi gi);

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
