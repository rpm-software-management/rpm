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

extern int _rpmal_debug;

/**
 * Initialize available packckages, items, and directory list.
 * @param delta		no. of entries to add on each realloc
 * @return al		new available list
 */
rpmal rpmalCreate(int delta);

/**
 * Free available packages, items, and directory members.
 * @param al		available list
 * @return		NULL always
 */
rpmal rpmalFree(rpmal al);

/**
 * Delete package from available list.
 * @param al		available list
 * @param p	        package
 */
void rpmalDel(rpmal al, rpmte p);

/**
 * Add package to available list.
 * @param alistp	address of available list
 * @param p             package
 * @param tscolor	transaction color bits
 * @return		available package index
 */
void rpmalAdd(rpmal * alistp,
	      rpmte p,
	      rpm_color_t tscolor);

/**
 * Generate index for available list.
 * @param al		available list
 */
void rpmalMakeIndex(rpmal al);

/**
 * Check added package file lists for package(s) that provide a file.
 * @param al		available list
 * @param ds		dependency set
 * @return		associated package(s), NULL if none
 */
rpmte * rpmalAllFileSatisfiesDepend(const rpmal al, const rpmds ds);

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package(s), NULL if none
 */
rpmte * rpmalAllSatisfiesDepend(const rpmal al, const rpmds ds);

/**
 * Check added package file lists for first package that has a provide.
 * @todo Eliminate.
 * @param al		available list
 * @param ds		dependency set
 * @return		associated package key, NULL if none
 */
rpmte rpmalSatisfiesDepend(const rpmal al, const rpmds ds);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
