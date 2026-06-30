#ifndef _RPMDS_INTERNAL_H
#define _RPMDS_INTERNAL_H

#include <rpm/rpmds.h>

/** \ingroup rpmds
 * Swiss army knife dependency matching function.
 * @param pool		string pool (or NULL for private pool)
 * @param h		header
 * @param prix		index to provides (or -1 or any)
 * @param req		dependency set
 * @param selfevr	only look at package EVR?
 * @return		1 if dependency overlaps, 0 otherwise
 */
int rpmdsMatches(rpmstrPool pool, Header h, int prix,
		 rpmds req, int selfevr);

/** \ingroup rpmds
 * Notify of results of dependency match.
 * @param ds		dependency set
 * @param where		where dependency was resolved (or NULL)
 * @param rc		0 == YES, otherwise NO
 */
void rpmdsNotify(rpmds ds, const char * where, int rc);

/** \ingroup rpmds
 * Return current dependency name pool id.
 * @param ds            dependency set
 * @return              current dependency name id, 0 on invalid
 */
rpmsid rpmdsNId(rpmds ds);

/** \ingroup rpmds
 * Return current dependency epoch-version-release pool id.
 * @param ds            dependency set
 * @return              current dependency EVR id, 0 on invalid
 */
rpmsid rpmdsEVRId(rpmds ds);

/** \ingroup rpmds
 * Return dependency set string pool handle
 * @param ds		dependency set
 * @return		string pool handle (weak reference)
 */
rpmstrPool rpmdsPool(rpmds ds);

rpmsid rpmdsNIdIndex(rpmds ds, int i);

rpmsid rpmdsEVRIdIndex(rpmds ds, int i);

const char * rpmdsNIndex(rpmds ds, int i);

const char * rpmdsEVRIndex(rpmds ds, int i);

rpmsenseFlags rpmdsFlagsIndex(rpmds ds, int i);

int rpmdsTiIndex(rpmds ds, int i);

rpm_color_t rpmdsColorIndex(rpmds ds, int i);

int rpmdsCompareIndex(rpmds A, int aix, rpmds B, int bix);

/** \ingroup rpmds
 * Filter dependency set and return new dependency set containing only items
 * with given trigger index.
 * @param ds		dependency set
 * @param ti		trigger index
 * @return		new filtered dependency set
 */
rpmds rpmdsFilterTi(rpmds ds, int ti);

#endif /* _RPMDS_INTERNAL_H */
