#ifndef H_RPMDS
#define H_RPMDS

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmds.h
 * Structure(s) used for dependency tag sets.
 */

#include <rpmlib.h>	/* for rpmds */
#include <rpmps.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
extern int _rpmds_debug;

/**
 */
extern int _rpmds_nopromote;

/**
 * Unreference a dependency set instance.
 * @param ds		dependency set
 * @param msg
 * @return		NULL always
 */
rpmds rpmdsUnlink (rpmds ds,
		const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmds XrpmdsUnlink (rpmds ds,
		const char * msg, const char * fn, unsigned ln);
#define	rpmdsUnlink(_ds, _msg)	XrpmdsUnlink(_ds, _msg, __FILE__, __LINE__)

/**
 * Reference a dependency set instance.
 * @param ds		dependency set
 * @param msg
 * @return		new dependency set reference
 */
rpmds rpmdsLink (rpmds ds, const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmds XrpmdsLink (rpmds ds, const char * msg,
		const char * fn, unsigned ln);
#define	rpmdsLink(_ds, _msg)	XrpmdsLink(_ds, _msg, __FILE__, __LINE__)

/**
 * Destroy a dependency set.
 * @param ds		dependency set
 * @return		NULL always
 */
rpmds rpmdsFree(rpmds ds);
/**
 * Create and load a dependency set.
 * @deprecated Only scareMem = 0 will be permitted.
 * @param h		header
 * @param tagN		type of dependency
 * @param flags		scareMem(0x1)
 * @return		new dependency set
 */
rpmds rpmdsNew(Header h, rpmTag tagN, int flags);

/**
 * Return new formatted dependency string.
 * @param dspfx		formatted dependency string prefix
 * @param ds		dependency set
 * @return		new formatted dependency (malloc'ed)
 */
char * rpmdsNewDNEVR(const char * dspfx, const rpmds ds);

/**
 * Create, load and initialize a dependency for this header. 
 * @param h		header
 * @param tagN		type of dependency
 * @param Flags		comparison flags
 * @return		new dependency set
 */
rpmds rpmdsThis(Header h, rpmTag tagN, int32_t Flags);

/**
 * Create, load and initialize a dependency set of size 1.
 * @param tagN		type of dependency
 * @param N		name
 * @param EVR		epoch:version-release
 * @param Flags		comparison flags
 * @return		new dependency set
 */
rpmds rpmdsSingle(rpmTag tagN, const char * N, const char * EVR, int32_t Flags);

/**
 * Return dependency set count.
 * @param ds		dependency set
 * @return		current count
 */
int rpmdsCount(const rpmds ds);

/**
 * Return dependency set index.
 * @param ds		dependency set
 * @return		current index
 */
int rpmdsIx(const rpmds ds);

/**
 * Set dependency set index.
 * @param ds		dependency set
 * @param ix		new index
 * @return		current index
 */
int rpmdsSetIx(rpmds ds, int ix);

/**
 * Return current formatted dependency string.
 * @param ds		dependency set
 * @return		current dependency DNEVR, NULL on invalid
 */
extern const char * rpmdsDNEVR(const rpmds ds);

/**
 * Return current dependency name.
 * @param ds		dependency set
 * @return		current dependency name, NULL on invalid
 */
extern const char * rpmdsN(const rpmds ds);

/**
 * Return current dependency epoch-version-release.
 * @param ds		dependency set
 * @return		current dependency EVR, NULL on invalid
 */
extern const char * rpmdsEVR(const rpmds ds);

/**
 * Return current dependency flags.
 * @param ds		dependency set
 * @return		current dependency flags, 0 on invalid
 */
int32_t rpmdsFlags(const rpmds ds);

/**
 * Return current dependency type.
 * @param ds		dependency set
 * @return		current dependency type, 0 on invalid
 */
rpmTag rpmdsTagN(const rpmds ds);

/**
 * Return dependency build time.
 * @param ds		dependency set
 * @return		dependency build time, 0 on invalid
 */
time_t rpmdsBT(const rpmds ds);

/**
 * Set dependency build time.
 * @param ds		dependency set
 * @param BT		build time
 * @return		dependency build time, 0 on invalid
 */
time_t rpmdsSetBT(const rpmds ds, time_t BT);

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
int rpmdsNoPromote(const rpmds ds);

/**
 * Set "Don't promote Epoch:" flag.
 * @param ds		dependency set
 * @param nopromote	Should an unspecified Epoch: be treated as Epoch: 0?
 * @return		previous "Don't promote Epoch:" flag
 */
int rpmdsSetNoPromote(rpmds ds, int nopromote);

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @return		current dependency color
 */
uint32_t rpmdsColor(const rpmds ds);

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @param color		new dependency color
 * @return		previous dependency color
 */
uint32_t rpmdsSetColor(const rpmds ds, uint32_t color);

/**
 * Return current dependency file refs.
 * @param ds		dependency set
 * @return		current dependency file refs, -1 on global
 */
int32_t rpmdsRefs(const rpmds ds);

/**
 * Return current dependency color.
 * @param ds		dependency set
 * @param refs		new dependency refs
 * @return		previous dependency refs
 */
int32_t rpmdsSetRefs(const rpmds ds, int32_t refs);

/**
 * Notify of results of dependency match.
 * @param ds		dependency set
 * @param where		where dependency was resolved (or NULL)
 * @param rc		0 == YES, otherwise NO
 */
/* FIX: rpmMessage annotation is a lie */
void rpmdsNotify(rpmds ds, const char * where, int rc);

/**
 * Return next dependency set iterator index.
 * @param ds		dependency set
 * @return		dependency set iterator index, -1 on termination
 */
int rpmdsNext(rpmds ds);

/**
 * Initialize dependency set iterator.
 * @param ds		dependency set
 * @return		dependency set
 */
rpmds rpmdsInit(rpmds ds);

/**
 * Find a dependency set element using binary search.
 * @param ds		dependency set to search
 * @param ods		dependency set element to find.
 * @return		dependency index (or -1 if not found)
 */
int rpmdsFind(rpmds ds, const rpmds ods);

/**
 * Merge a dependency set maintaining (N,EVR,Flags) sorted order.
 * @retval *dsp		(merged) dependency set
 * @param ods		dependency set to merge
 * @return		(merged) dependency index
 */
int rpmdsMerge(rpmds * dsp, rpmds ods);

/**
 * Compare two versioned dependency ranges, looking for overlap.
 * @param A		1st dependency
 * @param B		2nd dependency
 * @return		1 if dependencies overlap, 0 otherwise
 */
int rpmdsCompare(const rpmds A, const rpmds B);

/**
 * Report a Requires: or Conflicts: dependency problem.
 * @param ps		transaction set problems
 * @param pkgNEVR	package name/epoch/version/release
 * @param ds		dependency set
 * @param suggestedKeys	filename or python object address
 * @param adding	dependency problem is from added package set?
 */
void rpmdsProblem(rpmps ps, const char * pkgNEVR, const rpmds ds,
		const fnpyKey * suggestedKeys,
		int adding);

/**
 * Compare package provides dependencies from header with a single dependency.
 * @param h		header
 * @param req		dependency set
 * @param nopromote	Don't promote Epoch: in comparison?
 * @return		1 if any dependency overlaps, 0 otherwise
 */
int rpmdsAnyMatchesDep (const Header h, const rpmds req, int nopromote);

/**
 * Compare package name-version-release from header with a single dependency.
 * @param h		header
 * @param req		dependency set
 * @param nopromote	Don't promote Epoch: in comparison?
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmdsNVRMatchesDep(const Header h, const rpmds req, int nopromote);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDS */
