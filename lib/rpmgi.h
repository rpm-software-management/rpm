#ifndef	H_RPMGI
#define	H_RPMGI

/** \ingroup rpmio
 * \file lib/rpmgi.h
 */

#include <rpmlib.h>
#include <rpmts.h>
#include <fts.h>
#include <argv.h>

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmgi_debug;
/*@=exportlocal@*/

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

/*@unchecked@*/
extern rpmgiFlags giFlags;

#if defined(_RPMGI_INTERNAL)
/** \ingroup rpmio
 */
struct rpmgi_s {
/*@refcounted@*/
    rpmts ts;			/*!< Iterator transaction set. */
    int tag;			/*!< Iterator type. */
/*@kept@*/ /*@relnull@*/
    const void * keyp;		/*!< Iterator key. */
    size_t keylen;		/*!< Iterator key length. */

    rpmgiFlags flags;		/*!< Iterator control bits. */
    int active;			/*!< Iterator is active? */
    int i;			/*!< Element index. */
/*@null@*/
    const char * hdrPath;	/*!< Path to current iterator header. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Current iterator header. */

/*@null@*/
    rpmdbMatchIterator mi;

/*@refcounted@*/
    FD_t fd;

    ARGV_t argv;
    int argc;

    int ftsOpts;
/*@null@*/
    FTS * ftsp;
/*@relnull@*/
    FTSENT * fts;

/*@refs@*/
    int nrefs;			/*!< Reference count. */
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a generalized iterator instance.
 * @param gi		generalized iterator
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmgi rpmgiUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmgi gi,
		/*@null@*/ const char * msg)
	/*@modifies gi @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmgi XrpmgiUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmgi gi,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies gi @*/;
/*@=exportlocal@*/
#define	rpmgiUnlink(_gi, _msg)	XrpmgiUnlink(_gi, _msg, __FILE__, __LINE__)

/**
 * Reference a generalized iterator instance.
 * @param gi		generalized iterator
 * @param msg
 * @return		new generalized iterator reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmgi rpmgiLink (/*@null@*/ rpmgi gi, /*@null@*/ const char * msg)
	/*@modifies gi @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmgi XrpmgiLink (/*@null@*/ rpmgi gi, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies gi @*/;
#define	rpmgiLink(_gi, _msg)	XrpmgiLink(_gi, _msg, __FILE__, __LINE__)

/** Destroy a generalized iterator.
 * @param gi		generalized iterator
 * @return		NULL always
 */
/*@null@*/
rpmgi rpmgiFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
        /*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/;

/**
 * Return a generalized iterator.
 * @param ts		transaction set
 * @param tag		rpm tag
 * @param keyp		key data (NULL for sequential access)
 * @param keylen	key data length (0 will use strlen(keyp))
 * @return		new iterator
 */
/*@null@*/
rpmgi rpmgiNew(rpmts ts, int tag, /*@kept@*/ /*@null@*/ const void * keyp,
		size_t keylen)
	/*@globals internalState @*/
	/*@modifies ts, internalState @*/;

/**
 * Perform next iteration step.
 * @param gi		generalized iterator
 * @returns		RPMRC_OK on success, RPMRC_NOTFOUND on EOI
 */
rpmRC rpmgiNext(/*@null@*/ rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
        /*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/;

/**
 * Return current header path.
 * @param gi		generalized iterator
 * @returns		header path
 */
/*@observer@*/ /*@null@*/
const char * rpmgiHdrPath(rpmgi gi)
	/*@*/;

/**
 * Return current iteration header.
 * @param gi		generalized iterator
 * @returns		header
 */
/*@null@*/
Header rpmgiHeader(/*@null@*/ rpmgi gi)
        /*@*/;

/**
 * Return current iteration transaction set.
 * @param gi		generalized iterator
 * @returns		transaction set
 */
/*@null@*/
rpmts rpmgiTs(/*@null@*/ rpmgi gi)
        /*@*/;

/**
 * Load iterator args.
 * @param gi		generalized iterator
 * @param argv		arg list
 * @param ftsOpts	fts(3) flags
 * @param flags		iterator flags
 * @returns		RPMRC_OK on success
 */
rpmRC rpmgiSetArgs(rpmgi gi, /*@null@*/ ARGV_t argv,
		int ftsOpts, rpmgiFlags flags)
	/*@globals internalState @*/
	/*@modifies gi, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGI */
