#ifndef H_RPMSX
#define H_RPMSX

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmsx.h
 * Structure(s) used for file security context pattern handling
 */

#include <regex.h>
#include "selinux.h"

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
    const char * type;		/*!< File type string. */
/*@only@*/ /*@null@*/
    const char * context;	/*!< Security context. */
/*@only@*/ /*@null@*/
    regex_t * preg;		/*!< Compiled regex. */
    mode_t mode;		/*!< File type. */
    int matches;
    int hasMetaChars;
    int stem_id;
};

/**
 * File security context patterns container.
 */
struct rpmsx_s {
/*@only@*/ /*@null@*/
    rpmsxp sxp;			/*!< File context patterns. */
    int Count;			/*!< No. of elements */
    int i;			/*!< Current element index. */
    int reverse;		/*!< Reverse traversal? */
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
 * Parse selinux file security context patterns.
 * @param sx		security context patterns
 * @param fn		file name to parse
 * @return		0 on success
 */
int rpmsxParse(rpmsx sx, /*@null@*/ const char *fn)
	/*modifies sx @*/;

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
 * Return current pattern.
 * @param sx		security context patterns
 * @return		current pattern, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern const char * rpmsxPattern(/*@null@*/ const rpmsx sx)
	/*@*/;

/**
 * Return current type.
 * @param sx		security context patterns
 * @return		current type, NULL on invalid/missing
 */
/*@observer@*/ /*@null@*/
extern const char * rpmsxType(/*@null@*/ const rpmsx sx)
	/*@*/;

/**
 * Return current context.
 * @param sx		security context patterns
 * @return		current context, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern const char * rpmsxContext(/*@null@*/ const rpmsx sx)
	/*@*/;

/**
 * Return current regex.
 * @param sx		security context patterns
 * @return		current context, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern regex_t * rpmsxRE(/*@null@*/ const rpmsx sx)
	/*@*/;

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
 * @param reverse	iterate in reverse order?
 * @return		security context patterns
 */
/*@null@*/
rpmsx rpmsxInit(/*@null@*/ rpmsx sx, int reverse)
	/*@modifies sx @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSX */
