#ifndef H_RPMAL
#define H_RPMAL

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmal.h
 * Structures used for managing added/available package lists.
 */

typedef /*@abstract@*/ struct availableList_s *		availableList;

/**
 * Initialize available packckages, items, and directory list.
 * @param delta		no. of entries to add on each realloc
 * @return al		new available list
 */
/*@-exportlocal@*/
/*@only@*/
availableList alCreate(int delta)
	/*@*/;
/*@=exportlocal@*/

/**
 * Free available packages, items, and directory members.
 * @param al		available list
 * @return		NULL always
 */
/*@null@*/
availableList alFree(/*@only@*/ /*@null@*/ availableList al)
	/*@modifies al @*/;

/**
 * Delete package from available list.
 * @param al		available list
 * @param pkgKey	package key
 */
/*@-exportlocal@*/
void alDelPackage(/*@null@*/ availableList al, /*@null@*/ alKey pkgKey)
	/*@modifies al @*/;
/*@=exportlocal@*/

/**
 * Add package to available list.
 * @param alistp	address of available list
 * @param pkgKey	package key, RPMAL_NOMATCH to force an append
 * @param key		associated file name/python object
 * @param provides	provides dependency set
 * @param fi		file info set
 * @return		available package index
 */
alKey alAddPackage(availableList * alistp,
		/*@dependent@*/ /*@null@*/ alKey pkgKey,
		/*@dependent@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ rpmDepSet provides, /*@null@*/ TFI_t fi)
	/*@modifies *alistp, provides, fi @*/;

/**
 * Add package provides to available list index.
 * @param al		available list
 * @param pkgKey	package key
 * @param provides	added package provides
 */
/*@-exportlocal@*/
void alAddProvides(availableList al,
		/*@dependent@*/ /*@null@*/ alKey pkgKey,
		/*@null@*/ rpmDepSet provides)
	/*@modifies al, provides @*/;
/*@=exportlocal@*/

/**
 * Generate index for available list.
 * @param al		available list
 */
void alMakeIndex(/*@null@*/ availableList al)
	/*@modifies al @*/;

/**
 * Check added package file lists for package(s) that provide a file.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package key(s), NULL if none
 */
/*@-exportlocal@*/
/*@only@*/ /*@null@*/
fnpyKey * alAllFileSatisfiesDepend(const availableList al, const rpmDepSet ds,
		/*@null@*/ alKey * keyp)
	/*@globals fileSystem @*/
	/*@modifies al, *keyp, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package key(s), NULL if none
 */
/*@only@*/ /*@null@*/
fnpyKey * alAllSatisfiesDepend(const availableList al, const rpmDepSet ds,
		/*@null@*/ alKey * keyp)
	/*@globals fileSystem @*/
	/*@modifies al, *keyp, fileSystem @*/;

/**
 * Check added package file lists for first package that has a provide.
 * @todo Eliminate.
 * @param al		available list
 * @param ds		dependency set
 * @retval keyp		added package key pointer (or NULL)
 * @return		associated package key, NULL if none
 */
fnpyKey alSatisfiesDepend(const availableList al, const rpmDepSet ds,
		/*@null@*/ alKey * keyp)
	/*@globals fileSystem @*/
	/*@modifies al, *keyp, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
