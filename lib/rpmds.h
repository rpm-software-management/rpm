#ifndef H_RPMDS
#define H_RPMDS

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmds.h
 * Structure(s) used for dependency tag sets.
 */

#include "rpmps.h"

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmds_debug;
/*@=exportlocal@*/

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmds_nopromote;
/*@=exportlocal@*/

#if defined(_RPMDS_INTERNAL)
/**
 * A package dependency set.
 */
struct rpmds_s {
/*@observer@*/
    const char * Type;		/*!< Tag name. */
/*@only@*/ /*@null@*/
    const char * DNEVR;		/*!< Formatted dependency string. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Header for dependency set (or NULL) */
/*@only@*/
    const char ** N;		/*!< Name. */
/*@only@*/ /*@null@*/
    const char ** EVR;		/*!< Epoch-Version-Release. */
/*@only@*/ /*@null@*/
    int_32 * Flags;		/*!< Bit(s) identifying context/comparison. */
/*@only@*/ /*@null@*/
    uint_32 * Color;		/*!< Bit(s) calculated from file color(s). */
/*@only@*/ /*@null@*/
    int_32 * Refs;		/*!< No. of file refs. */
    int_32 BT;			/*!< Package build time tie breaker. */
    rpmTag tagN;		/*!< Header tag. */
    rpmTagType Nt, EVRt, Ft;	/*!< Tag data types. */
    int_32 Count;		/*!< No. of elements */
    int i;			/*!< Element index. */
    unsigned l;			/*!< Low element (bsearch). */
    unsigned u;			/*!< High element (bsearch). */
    int nopromote;		/*!< Don't promote Epoch: in rpmdsCompare()? */
/*@refs@*/
    int nrefs;			/*!< Reference count. */
};
#endif	/* _RPMDS_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a dependency set instance.
 * @param ds		dependency set
 * @param msg
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmds rpmdsUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmds ds,
		/*@null@*/ const char * msg)
	/*@modifies ds @*/;

/** @todo Remove debugging entry from the ABI. */
/*@-exportlocal@*/
/*@null@*/
rpmds XrpmdsUnlink (/*@killref@*/ /*@only@*/ /*@null@*/ rpmds ds,
		/*@null@*/ const char * msg, const char * fn, unsigned ln)
	/*@modifies ds @*/;
/*@=exportlocal@*/
#define	rpmdsUnlink(_ds, _msg)	XrpmdsUnlink(_ds, _msg, __FILE__, __LINE__)

/**
 * Reference a dependency set instance.
 * @param ds		dependency set
 * @param msg
 * @return		new dependency set reference
 */
/*@unused@*/ /*@newref@*/ /*@null@*/
rpmds rpmdsLink (/*@null@*/ rpmds ds, /*@null@*/ const char * msg)
	/*@modifies ds @*/;

/** @todo Remove debugging entry from the ABI. */
/*@newref@*/ /*@null@*/
rpmds XrpmdsLink (/*@null@*/ rpmds ds, /*@null@*/ const char * msg,
		const char * fn, unsigned ln)
        /*@modifies ds @*/;
#define	rpmdsLink(_ds, _msg)	XrpmdsLink(_ds, _msg, __FILE__, __LINE__)

/**
 * Destroy a dependency set.
 * @param ds		dependency set
 * @return		NULL always
 */
/*@null@*/
rpmds rpmdsFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmds ds)
	/*@modifies ds@*/;
/**
 * Create and load a dependency set.
 * @deprecated Only scareMem = 0 will be permitted.
 * @param h		header
 * @param tagN		type of dependency
 * @param flags		scareMem(0x1), nofilter(0x2)
 * @return		new dependency set
 */
/*@null@*/
rpmds rpmdsNew(Header h, rpmTag tagN, int flags)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Return new formatted dependency string.
 * @param dspfx		formatted dependency string prefix
 * @param ds		dependency set
 * @return		new formatted dependency (malloc'ed)
 */
/*@only@*/
char * rpmdsNewDNEVR(const char * dspfx, const rpmds ds)
	/*@*/;

/**
 * Create, load and initialize a dependency for this header. 
 * @param h		header
 * @param tagN		type of dependency
 * @param Flags		comparison flags
 * @return		new dependency set
 */
/*@null@*/
rpmds rpmdsThis(Header h, rpmTag tagN, int_32 Flags)
	/*@*/;

/**
 * Create, load and initialize a dependency set of size 1.
 * @param tagN		type of dependency
 * @param N		name
 * @param EVR		epoch:version-release
 * @param Flags		comparison flags
 * @return		new dependency set
 */
/*@null@*/
rpmds rpmdsSingle(rpmTag tagN, const char * N, const char * EVR, int_32 Flags)
	/*@*/;

/**
 * Return dependency set count.
 * @param ds		dependency set
 * @return		current count
 */
int rpmdsCount(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return dependency set index.
 * @param ds		dependency set
 * @return		current index
 */
int rpmdsIx(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set dependency set index.
 * @param ds		dependency set
 * @param ix		new index
 * @return		current index
 */
int rpmdsSetIx(/*@null@*/ rpmds ds, int ix)
	/*@modifies ds @*/;

/**
 * Return current formatted dependency string.
 * @param ds		dependency set
 * @return		current dependency DNEVR, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern const char * rpmdsDNEVR(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency name.
 * @param ds		dependency set
 * @return		current dependency name, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern const char * rpmdsN(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency epoch-version-release.
 * @param ds		dependency set
 * @return		current dependency EVR, NULL on invalid
 */
/*@observer@*/ /*@null@*/
extern const char * rpmdsEVR(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency flags.
 * @param ds		dependency set
 * @return		current dependency flags, 0 on invalid
 */
int_32 rpmdsFlags(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency type.
 * @param ds		dependency set
 * @return		current dependency type, 0 on invalid
 */
rpmTag rpmdsTagN(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return dependency build time.
 * @param ds		dependency set
 * @return		dependency build time, 0 on invalid
 */
time_t rpmdsBT(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set dependency build time.
 * @param ds		dependency set
 * @param BT		build time
 * @return		dependency build time, 0 on invalid
 */
time_t rpmdsSetBT(/*@null@*/ const rpmds ds, time_t BT)
	/*@modifies ds @*/;

/**
 * Return current "Don't promote Epoch:" flag.
 *
 * This flag controls for Epoch: promotion when a dependency set is
 * compared. If the flag is set (for already installed packages), then
 * an unspecified value will be treated as Epoch: 0. Otherwise (for added
 * packages), the Epoch: portion of the comparison is skipped if the value
 * is not specified, i.e. an unspecified Epoch: is assumed to be equal
 * in dependency comparisons.
 *
 * @param ds		dependency set
 * @return		current "Don't promote Epoch:" flag
 */
int rpmdsNoPromote(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Set "Don't promote Epoch:" flag.
 * @param ds		dependency set
 * @param nopromote	Should an unspecified Epoch: be treated as Epoch: 0?
 * @return		previous "Don't promote Epoch:" flag
 */
int rpmdsSetNoPromote(/*@null@*/ rpmds ds, int nopromote)
	/*@modifies ds @*/;

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @return		current dependency color
 */
uint_32 rpmdsColor(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @param color		new dependency color
 * @return		previous dependency color
 */
uint_32 rpmdsSetColor(/*@null@*/ const rpmds ds, uint_32 color)
	/*@modifies ds @*/;

/**
 * Return current dependency file refs.
 * @param ds		dependency set
 * @return		current dependency file refs, -1 on global
 */
int_32 rpmdsRefs(/*@null@*/ const rpmds ds)
	/*@*/;

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @param refs		new dependency refs
 * @return		previous dependency refs
 */
int_32 rpmdsSetRefs(/*@null@*/ const rpmds ds, int_32 refs)
	/*@modifies ds @*/;

/**
 * Notify of results of dependency match.
 * @param ds		dependency set
 * @param where		where dependency was resolved (or NULL)
 * @param rc		0 == YES, otherwise NO
 */
/*@-globuse@*/ /* FIX: rpmMessage annotation is a lie */
void rpmdsNotify(/*@null@*/ rpmds ds, /*@null@*/ const char * where, int rc)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@=globuse@*/

/**
 * Return next dependency set iterator index.
 * @param ds		dependency set
 * @return		dependency set iterator index, -1 on termination
 */
int rpmdsNext(/*@null@*/ rpmds ds)
	/*@modifies ds @*/;

/**
 * Initialize dependency set iterator.
 * @param ds		dependency set
 * @return		dependency set
 */
/*@null@*/
rpmds rpmdsInit(/*@null@*/ rpmds ds)
	/*@modifies ds @*/;

/**
 * Find a dependency set element using binary search.
 * @param ds		dependency set to search
 * @param ods		dependency set element to find.
 * @return		dependency index (or -1 if not found)
 */
/*@null@*/
int rpmdsFind(rpmds ds, /*@null@*/ const rpmds ods)
	/*@modifies ds @*/;

/**
 * Merge a dependency set maintaining (N,EVR,Flags) sorted order.
 * @retval *dsp		(merged) dependency set
 * @param ods		dependency set to merge
 * @return		(merged) dependency index
 */
/*@null@*/
int rpmdsMerge(/*@out@*/ rpmds * dsp, /*@null@*/ rpmds ods)
	/*@modifies *dsp, ods @*/;

/**
 * Compare two versioned dependency ranges, looking for overlap.
 * @param A		1st dependency
 * @param B		2nd dependency
 * @return		1 if dependencies overlap, 0 otherwise
 */
int rpmdsCompare(const rpmds A, const rpmds B)
	/*@*/;

/**
 * Report a Requires: or Conflicts: dependency problem.
 * @param ps		transaction set problems
 * @param pkgNEVR	package name/epoch/version/release
 * @param ds		dependency set
 * @param suggestedKeys	filename or python object address
 * @param adding	dependency problem is from added package set?
 */
void rpmdsProblem(/*@null@*/ rpmps ps, const char * pkgNEVR, const rpmds ds,
		/*@only@*/ /*@null@*/ const fnpyKey * suggestedKeys,
		int adding)
	/*@modifies ps @*/;

/**
 * Compare package provides dependencies from header with a single dependency.
 * @param h		header
 * @param req		dependency set
 * @param nopromote	Don't promote Epoch: in comparison?
 * @return		1 if any dependency overlaps, 0 otherwise
 */
int rpmdsAnyMatchesDep (const Header h, const rpmds req, int nopromote)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Compare package name-version-release from header with a single dependency.
 * @param h		header
 * @param req		dependency set
 * @param nopromote	Don't promote Epoch: in comparison?
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmdsNVRMatchesDep(const Header h, const rpmds req, int nopromote)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDS */
