#ifndef H_RPMSX
#define H_RPMSX

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmsx.h
 * Structure(s) used for file security context pattern handling
 */

#include <regex.h>

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmsx_debug;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmsx_nopromote;
/*@=exportlocal@*/

typedef struct rpmsxp_s * rpmsxp;
typedef struct rpmsx_s * rpmsx;

#if defined(_RPMSX_INTERNAL)
/**
 * File security context regex pattern.
 */
struct rpmsxp_s {
/*@only@*/ /*@null@*/
    const char * pattern;	/*!< File path regex pattern. */
/*@only@*/ /*@null@*/
    const char * type;		/*!< File type. */
/*@only@*/ /*@null@*/
    const char * context;	/*!< Security context. */
/*@only@*/ /*@null@*/
    regex_t * preg;		/*!< Compiled regex. */
};

/**
 * File security context patterns container.
 */
struct rpmsx_s {
    int Count;			/*!< No. of elements */
    int i;			/*!< Current element index. */
/*@only@*/ /*@null@*/
    rpmsxp sxp;			/*!< File context patterns. */
    int nrefs;			/*!< Reference count. */
};
#endif /* defined(_RPMSX_INTERNAL) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a security context patterns instance.
 * @param sx		security context patterns
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmsx rpmsxUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsx sx,
		/*@null@*/ const char * msg)
	/*@modifies sx @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmsx XrpmsxUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmsx sx,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies sx @*/;
/*@=exportlocal@*/
#define	rpmsxUnlink(_sx, _msg)	XrpmsxUnlink(_sx, _msg, __FILE__, __LINE__)

/**
 * Reference a security context patterns instance.
 * @param sx		security context patterns
 * @param msg
 * @return		new security context patterns reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsx rpmsxLink (/*@null@*/ rpmsx sx, /*@null@*/ const char * msg)
	/*@modifies sx @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmsx XrpmsxLink (/*@null@*/ rpmsx sx, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies sx @*/;
#define	rpmsxLink(_sx, _msg)	XrpmsxLink(_sx, _msg, __FILE__, __LINE__)

/**
 * Destroy a security context patterns.
 * @param sx		security context patterns
 * @return		NULL always
 */
/*@null@*/
rpmsx rpmsxFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmsx sx)
	/*@modifies sx@*/;
/**
 * Create and load security context patterns.
 * @param fn		header
 * @param tagN		type of dependency
 * @param scareMem	Use pointers to refcounted header memory?
 * @return		new security context patterns
 */
/*@null@*/
rpmsx rpmsxNew(const char * fn)
	/*@*/;

/**
 * Return security context patterns count.
 * @param sx		security context patterns
 * @return		current count
 */
int rpmsxCount(/*@null@*/ const rpmsx sx)
	/*@*/;

/**
 * Return security context patterns index.
 * @param sx		security context patterns
 * @return		current index
 */
int rpmsxIx(/*@null@*/ const rpmsx sx)
	/*@*/;

/**
 * Set security context patterns index.
 * @param sx		security context patterns
 * @param ix		new index
 * @return		current index
 */
int rpmsxSetIx(/*@null@*/ rpmsx sx, int ix)
	/*@modifies sx @*/;

/**
 * Return next security context patterns iterator index.
 * @param sx		security context patterns
 * @return		security context patterns iterator index, -1 on termination
 */
int rpmsxNext(/*@null@*/ rpmsx sx)
	/*@modifies sx @*/;

/**
 * Initialize security context patterns iterator.
 * @param sx		security context patterns
 * @return		security context patterns
 */
/*@null@*/
rpmsx rpmsxInit(/*@null@*/ rpmsx sx)
	/*@modifies sx @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSX */
