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

typedef /*@abstract@*/ /*@refcounted@*/ struct rpmsx_s * rpmsx;
typedef struct rpmsxp_s * rpmsxp;
typedef struct rpmsxs_s * rpmsxs;

#if defined(_RPMSX_INTERNAL)
/**
 * File security context regex pattern.
 */
struct rpmsxp_s {
/*@only@*/ /*@relnull@*/
    const char * pattern;	/*!< File path regex pattern. */
/*@only@*/ /*@relnull@*/
    const char * type;		/*!< File type string. */
/*@only@*/ /*@relnull@*/
    const char * context;	/*!< Security context. */
/*@only@*/ /*@relnull@*/
    regex_t * preg;		/*!< Compiled regex. */
    mode_t fmode;		/*!< File type. */
    int matches;
    int hasMetaChars;
    int fstem;			/*!< Stem id. */
};

/**
 * File/pattern stem.
 */
struct rpmsxs_s {
/*@only@*/ /*@relnull@*/
    const char * stem;
    int len;
};

/**
 * File security context patterns container.
 */
struct rpmsx_s {
/*@only@*/ /*@relnull@*/
    rpmsxp sxp;			/*!< File context patterns. */
    int Count;			/*!< No. of file context patterns. */
    int i;			/*!< Current pattern index. */
/*@only@*/ /*@relnull@*/
    rpmsxs sxs;			/*!< File stems. */
    int nsxs;			/*!< No. of file stems. */
    int maxsxs;			/*!< No. of allocated file stems. */
    int reverse;		/*!< Reverse traversal? */
/*@refs@*/
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
/*@-exportlocal@*/
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmsx rpmsxLink (/*@null@*/ rpmsx sx, /*@null@*/ const char * msg)
	/*@modifies sx @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmsx XrpmsxLink (/*@null@*/ rpmsx sx, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies sx @*/;
/*@=exportlocal@*/
#define	rpmsxLink(_sx, _msg)	XrpmsxLink(_sx, _msg, __FILE__, __LINE__)

/**
 * Destroy a security context patterns.
 * @param sx		security context patterns
 * @return		NULL always
 */
/*@-exportlocal@*/
/*@null@*/
rpmsx rpmsxFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmsx sx)
	/*@modifies sx@*/;
/*@=exportlocal@*/

/**
 * Parse selinux file security context patterns.
 * @param sx		security context patterns
 * @param fn		file name to parse
 * @return		0 on success
 */
/*@-exportlocal@*/
int rpmsxParse(rpmsx sx, /*@null@*/ const char *fn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies sx, rpmGlobalMacroContext, h_errno, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Create and load security context patterns.
 * @param fn		security context patterns file name
 * @return		new security context patterns
 */
/*@null@*/
rpmsx rpmsxNew(const char * fn)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem @*/
	/*@modifies rpmGlobalMacroContext, h_errno, fileSystem @*/;

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
/*@-exportlocal@*/
/*@observer@*/ /*@null@*/
extern const char * rpmsxPattern(/*@null@*/ const rpmsx sx)
	/*@*/;
/*@=exportlocal@*/

/**
 * Return current type.
 * @param sx		security context patterns
 * @return		current type, NULL on invalid/missing
 */
/*@-exportlocal@*/
/*@observer@*/ /*@null@*/
extern const char * rpmsxType(/*@null@*/ const rpmsx sx)
	/*@*/;
/*@=exportlocal@*/

/**
 * Return current context.
 * @param sx		security context patterns
 * @return		current context, NULL on invalid
 */
/*@-exportlocal@*/
/*@observer@*/ /*@null@*/
extern const char * rpmsxContext(/*@null@*/ const rpmsx sx)
	/*@*/;
/*@=exportlocal@*/

/**
 * Return current regex.
 * @param sx		security context patterns
 * @return		current context, NULL on invalid
 */
/*@-exportlocal@*/
/*@observer@*/ /*@null@*/
extern regex_t * rpmsxRE(/*@null@*/ const rpmsx sx)
	/*@*/;
/*@=exportlocal@*/

/**
 * Return current file mode.
 * @param sx		security context patterns
 * @return		current file mode, 0 on invalid
 */
/*@-exportlocal@*/
extern mode_t rpmsxFMode(/*@null@*/ const rpmsx sx)
	/*@*/;
/*@=exportlocal@*/

/**
 * Return current file stem.
 * @param sx		security context patterns
 * @return		current file stem, -1 on invalid
 */
/*@-exportlocal@*/
extern int rpmsxFStem(/*@null@*/ const rpmsx sx)
	/*@*/;
/*@=exportlocal@*/

/**
 * Return next security context patterns iterator index.
 * @param sx		security context patterns
 * @return		security context patterns iterator index, -1 on termination
 */
/*@-exportlocal@*/
int rpmsxNext(/*@null@*/ rpmsx sx)
	/*@modifies sx @*/;
/*@=exportlocal@*/

/**
 * Initialize security context patterns iterator.
 * @param sx		security context patterns
 * @param reverse	iterate in reverse order?
 * @return		security context patterns
 */
/*@-exportlocal@*/
/*@null@*/
rpmsx rpmsxInit(/*@null@*/ rpmsx sx, int reverse)
	/*@modifies sx @*/;
/*@=exportlocal@*/

/**
 * Find file security context from path and type.
 * @param sx		security context patterns
 * @param fn		file path
 * @param fmode		file mode
 * @return		file security context
 */
/*@owned@*/ /*@null@*/
const char * rpmsxFContext(/*@null@*/ rpmsx sx, const char * fn, mode_t fmode)
	/*@modifies sx @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMSX */
