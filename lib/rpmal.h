#ifndef H_RPMAL
#define H_RPMAL

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmal.h
 * Structures used for managing added/available package lists.
 */

/**
 * A package from an availableList.
 */
typedef /*@abstract@*/ struct availablePackage_s * availablePackage;

/**
 * Return number of packages in list.
 * @param al		available list
 * @return		no. of packages in list
 */
int alGetSize(const availableList al)
	/*@*/;

/**
 * Return available package header.
 * @param al		available list
 * @param pkgKey	available package key
 * @param unlink	Should alp->h be unlinked?
 * @return		available package header
 */
Header alGetHeader(/*@null@*/ availableList al, /*@null@*/ alKey pkgKey,
		int unlink)
	/*@modifies al @*/;

/**
 * Return available package.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package pointer
 */
/*@-exportlocal@*/
/*@dependent@*/ /*@null@*/
availablePackage alGetPkg(/*@null@*/ availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;
/*@=exportlocal@*/

#ifdef	DYING
/**
 * Return (malloc'd) available package name-version-release string.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		name-version-release string
 */
/*@only@*/ /*@null@*/
char * alGetNVR(/*@null@*/const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;
#endif

/**
 * Initialize available packckages, items, and directory list.
 * @param delta		no. of entries to add on each realloc
 * @return al		new available list
 */
/*@only@*/
availableList alCreate(int delta)
	/*@*/;

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
void alDelPackage(availableList al, /*@null@*/ alKey pkgKey)
	/*@modifies al @*/;
/*@=exportlocal@*/

/**
 * Add package to available list.
 * @param al		available list
 * @param pkgKey	package key, RPMAL_NOMATCH to force an append
 * @param key		associated file name/python object
 * @param h		package header
 * @param provides	provides dependency set
 * @param fns		file info set
 * @return		available package index
 */
alKey alAddPackage(availableList al, /*@null@*/ alKey pkgKey,
		fnpyKey key, Header h, rpmDepSet provides, rpmFNSet fns)
	/*@modifies al, h, provides, fns @*/;

/**
 * Add package provides to available list index.
 * @param al		available list
 * @param pkgKey	package key
 * @param provides	added package provides
 */
/*@-exportlocal@*/
void alAddProvides(availableList al, /*@null@*/ alKey pkgKey,
		/*@null@*/ rpmDepSet provides)
	/*@modifies al, provides @*/;
/*@=exportlocal@*/

/**
 * Generate index for available list.
 * @param al		available list
 */
void alMakeIndex(availableList al)
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
