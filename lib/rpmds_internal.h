#ifndef _RPMDS_INTERNAL_H
#define _RPMDS_INTERNAL_H

#include <rpm/rpmds.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmds
 * Create and load a dependency set.
 * @param pool		shared string pool (or NULL for private pool)
 * @param h		header
 * @param tagN		type of dependency
 * @param flags		unused
 * @return		new dependency set
 */
RPM_GNUC_INTERNAL
rpmds rpmdsNewPool(rpmstrPool pool, Header h, rpmTagVal tagN, int flags);

/** \ingroup rpmds
 * Create, load and initialize a dependency for this header. 
 * @param pool		string pool (or NULL for private pool)
 * @param h		header
 * @param tagN		type of dependency
 * @param Flags		comparison flags
 * @return		new dependency set
 */
RPM_GNUC_INTERNAL
rpmds rpmdsThisPool(rpmstrPool pool,
		    Header h, rpmTagVal tagN, rpmsenseFlags Flags);

/** \ingroup rpmds
 * Create, load and initialize a dependency set of size 1.
 * @param pool		string pool (or NULL for private pool)
 * @param tagN		type of dependency
 * @param N		name
 * @param EVR		epoch:version-release
 * @param Flags		comparison flags
 * @return		new dependency set
 */
RPM_GNUC_INTERNAL
rpmds rpmdsSinglePool(rpmstrPool pool, rpmTagVal tagN,
		      const char * N, const char * EVR, rpmsenseFlags Flags);

/**
 * Load rpmlib provides into a dependency set.
 * @param pool		shared string pool (or NULL for private pool)
 * @retval *dsp		(loaded) depedency set
 * @param tblp		rpmlib provides table (NULL uses internal table)
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int rpmdsRpmlibPool(rpmstrPool pool, rpmds * dsp, const void * tblp);

/** \ingroup rpmds
 * Swiss army knife dependency matching function.
 * @param pool		string pool (or NULL for private pool)
 * @param h		header
 * @param prix		index to provides (or -1 or any)
 * @param req		dependency set
 * @param selfevr	only look at package EVR?
 * @param nopromote	dont promote epoch in comparison?
 * @return		1 if dependency overlaps, 0 otherwise
 */
RPM_GNUC_INTERNAL
int rpmdsMatches(rpmstrPool pool, Header h, int prix,
		 rpmds req, int selfevr, int nopromote);

/** \ingroup rpmds
 * Return current dependency name pool id.
 * @param ds            dependency set
 * @return              current dependency name id, 0 on invalid
 */
RPM_GNUC_INTERNAL
rpmsid rpmdsNId(rpmds ds);

/** \ingroup rpmds
 * Return current dependency epoch-version-release pool id.
 * @param ds            dependency set
 * @return              current dependency EVR id, 0 on invalid
 */
RPM_GNUC_INTERNAL
rpmsid rpmdsEVRId(rpmds ds);

/** \ingroup rpmds
 * Return dependency set string pool handle
 * @param ds		dependency set
 * @return		string pool handle (weak reference)
 */
RPM_GNUC_INTERNAL
rpmstrPool rpmdsPool(rpmds ds);

RPM_GNUC_INTERNAL
rpmsid rpmdsNIdIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
rpmsid rpmdsEVRIdIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
const char * rpmdsNIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
const char * rpmdsEVRIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
rpmsenseFlags rpmdsFlagsIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
rpm_color_t rpmdsColorIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
int rpmdsCompareIndex(rpmds A, int aix, rpmds B, int bix);

#ifdef __cplusplus
}
#endif

#endif /* _RPMDS_INTERNAL_H */
