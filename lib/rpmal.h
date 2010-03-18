#ifndef H_RPMAL
#define H_RPMAL

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmal.h
 * Structures used for managing added/available package lists.
 */

#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpmal_s * rpmal;

/**
 * Initialize available packckages, items, and directory list.
 * @param delta		no. of entries to add on each realloc
 * @param tscolor	transaction color bits
 * @param prefcolor	preferred color
 * @return al		new available list
 */
RPM_GNUC_INTERNAL
rpmal rpmalCreate(int delta, rpm_color_t tscolor, rpm_color_t prefcolor);

/**
 * Free available packages, items, and directory members.
 * @param al		available list
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
rpmal rpmalFree(rpmal al);

/**
 * Delete package from available list.
 * @param al		available list
 * @param p	        package
 */
RPM_GNUC_INTERNAL
void rpmalDel(rpmal al, rpmte p);

/**
 * Add package to available list.
 * @param al	        available list
 * @param p             package
 */
RPM_GNUC_INTERNAL
void rpmalAdd(rpmal al, rpmte p);

/**
 * Lookup best provider for a dependency in the available list
 * @param al		available list
 * @param ds		dependency set
 * @return		best provider for the dependency, NULL if none
 */
RPM_GNUC_INTERNAL
rpmte rpmalSatisfiesDepend(const rpmal al, const rpmds ds);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
