#ifndef H_RPMAL
#define H_RPMAL

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmal.h
 * Structures used for managing added/available package lists.
 */

#include <rpmlib.h>		/* for rpmds, rpmfi, uint32_t */
#include <rpmmessages.h>	/* for fnpyKey */

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpmal_debug;

/**
 */
typedef struct rpmal_s *		rpmal;

/** \ingroup rpmtrans
 *  * An added/available package retrieval key.
 *   */
typedef void * rpmalKey;
#define RPMAL_NOMATCH   ((rpmalKey)-1L)

/** \ingroup rpmtrans
 *  * An added/available package retrieval index.
 *   */
typedef intptr_t rpmalNum;

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
 * @param pkgKey	package key
 */
void rpmalDel(rpmal al, rpmalKey pkgKey);

/**
 * Add package to available list.
 * @param alistp	address of available list
 * @param pkgKey	package key, RPMAL_NOMATCH to force an append
 * @param key		associated file name/python object
 * @param provides	provides dependency set
 * @param fi		file info set
 * @param tscolor	transaction color bits
 * @return		available package index
 */
rpmalKey rpmalAdd(rpmal * alistp,
		rpmalKey pkgKey,
		fnpyKey key,
		rpmds provides, rpmfi fi,
		uint32_t tscolor);

/**
 * Add package provides to available list index.
 * @param al		available list
 * @param pkgKey	package key
 * @param provides	added package provides
 * @param tscolor	transaction color bits
 */
void rpmalAddProvides(rpmal al,
		rpmalKey pkgKey,
		rpmds provides, uint32_t tscolor);

/**
 * Generate index for available list.
 * @param al		available list
 */
void rpmalMakeIndex(rpmal al);

/**
 * Check added package file lists for package(s) that provide a file.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package key(s), NULL if none
 */
fnpyKey * rpmalAllFileSatisfiesDepend(const rpmal al,
		const rpmds ds, rpmalKey * keyp);

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package key(s), NULL if none
 */
fnpyKey * rpmalAllSatisfiesDepend(const rpmal al, const rpmds ds,
		rpmalKey * keyp);

/**
 * Check added package file lists for first package that has a provide.
 * @todo Eliminate.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package key, NULL if none
 */
fnpyKey rpmalSatisfiesDepend(const rpmal al, const rpmds ds,
		rpmalKey * keyp);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
