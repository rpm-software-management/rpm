#ifndef _RPMVER_H
#define _RPMVER_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmds.h>		/* sense flags */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmver
 * Segmented string compare for version or release strings.
 *
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
RPM_PUBLIC_API
int rpmvercmp(const char * a, const char * b);

/** \ingroup rpmver
 * Parse rpm version handle from evr string
 *
 * @param evr		[epoch:]version[-release] string
 * @return		rpm version, NULL on invalid evr
 */
RPM_PUBLIC_API
rpmver rpmverParse(const char *evr);

/** \ingroup rpmver
 * Create new rpm version handle from e, v, r components
 *
 * @param e		epoch (or NULL)
 * @param v		version
 * @param r		release (or NULL)
 * @return		rpm version, NULL on invalid
 */
RPM_PUBLIC_API
rpmver rpmverNew(const char *e, const char *v, const char *r);

/** \ingroup rpmver
 * Free rpm version handle
 *
 * @param rv		rpm version handle
 * @return		NULL always
 */
RPM_PUBLIC_API
rpmver rpmverFree(rpmver rv);

/** \ingroup rpmver
 * @param rv		rpm version handle
 * @return		numerical value of epoch
 */
RPM_PUBLIC_API
uint32_t rpmverEVal(rpmver rv);

/** \ingroup rpmver
 * @param rv		rpm version handle
 * @return		epoch portion
 */
RPM_PUBLIC_API
const char *rpmverE(rpmver rv);

/** \ingroup rpmver
 * @param rv		rpm version handle
 * @return		version portion
 */
RPM_PUBLIC_API
const char *rpmverV(rpmver rv);

/** \ingroup rpmver
 * @param rv		rpm version handle
 * @return		release portion
 */
RPM_PUBLIC_API
const char *rpmverR(rpmver rv);

/** \ingroup rpmver
 * @param rv		rpm version handle
 * @return		formatted [E:]V[-R] string (malloced)
 */
RPM_PUBLIC_API
char *rpmverEVR(rpmver rv);

/** \ingroup rpmver
 * Compare two rpm version handles
 *
 * @param v1		1st version handle
 * @param v2		2nd version handle
 * @return		0 if equal, -1 if v1 smaller, 1 if greater, than v2
 */
RPM_PUBLIC_API
int rpmverCmp(rpmver v1, rpmver v2);

/** \ingroup rpmver
 * Determine whether two versioned ranges overlap.
 * @param v1		1st version
 * @param f1		1st sense flags
 * @param v2		2nd version
 * @param f2		2nd sense flags
 * @return		1 if ranges overlap, 0 otherwise
 */
RPM_PUBLIC_API
int rpmverOverlap(rpmver v1, rpmsenseFlags f1, rpmver v2, rpmsenseFlags f2);

#ifdef __cplusplus
}
#endif

#endif /* _RPMVER_H */
