#ifndef	H_RPMGI
#define	H_RPMGI

/** \ingroup rpmio
 * \file rpmio/rpmgi.h
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
 * Iterator types.
 */
typedef enum rpmgiTag_e {
    RPMGI_RPMDB		= RPMDBI_PACKAGES,
    RPMGI_HDLIST	= 6,	/* XXX next after RPMDBI_AVAILABLE */
    RPMGI_ARGLIST	= 7,
    RPMGI_FTSWALK	= 8
} rpmgiTag;

#if defined(_RPMGI_INTERNAL)
/** \ingroup rpmio
 */
struct rpmgi_s {
/*@refcounted@*/
    rpmts ts;			/*!< Iterator transaction set. */
    int tag;			/*!< Iterator type. */
    int active;			/*!< Iterator is initialized? */
    int i;			/*!< Element index. */
    const char * queryFormat;	/*!< Iterator query format. */
    const char * hdrPath;	/*!< Path to header objects. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Current iterator header. */

/*@relnull@*/
    rpmdbMatchIterator mi;

/*@refcounted@*/
    FD_t fd;

    ARGV_t argv;
    int argc;

    int ftsOpts;
/*@null@*/
    FTS * ftsp;
/*@null@*/
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

/** Create a generalized iterator.
 * @param argv		iterator argv array
 * @param flags		iterator flags
 * @return		new general iterator
 */
/*@null@*/
rpmgi rpmgiNew(rpmts ts, int tag, void *const keyp, size_t keylen)
	/*@globals internalState @*/
	/*@modifies ts, internalState @*/;

/**
 * Return next iteration element.
 * @param gi		generalized iterator
 * @returns		next element
 */
/*@observer@*/
const char * rpmgiNext(/*@null@*/ rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
        /*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/;

/**
 * Return iterator query format.
 * @param gi		generalized iterator
 * @returns		query format
 */
/*@observer@*/
const char * rpmgiQueryFormat(rpmgi gi)
	/*@*/;

/**
 * Set iterator query format.
 * @param gi		generalized iterator
 * @param queryFormat	query format
 * @returns		0
 */
int rpmgiSetQueryFormat(rpmgi gi, const char * queryFormat)
	/*@modifies gi @*/;

/**
 * Return iterator header objects path.
 * @param gi		generalized iterator
 * @returns		header path
 */
/*@observer@*/
const char * rpmgiHdrPath(rpmgi gi)
	/*@*/;

/**
 * Set iterator header objects path.
 * @param gi		generalized iterator
 * @param hdrPath	header path
 * @returns		0
 */
int rpmgiSetHdrPath(rpmgi gi, const char * hdrPath)
	/*@modifies gi @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMGI */
