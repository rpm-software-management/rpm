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

#ifdef	DYING
/**
 * Return available package multiLib flag.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package multiLib flag
 */
int alGetMultiLib(/*@null@*/ const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;
#endif

#ifndef	DYING
/**
 * Return available package files count.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package files count
 */
int alGetFilesCount(/*@null@*/ const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;
#endif

/**
 * Return available package provides.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package provides
 */
/*@null@*/
rpmDepSet alGetProvides(/*@null@*/ const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;

/**
 * Return available package requires.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package requires
 */
/*@null@*/
rpmDepSet alGetRequires(/*@null@*/ const availableList al, /*@null@*/ alKey pkgKey)
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

/**
 * Return (malloc'd) available package name-version-release string.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		name-version-release string
 */
/*@only@*/ /*@null@*/
char * alGetNVR(/*@null@*/const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;

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
 * @param h		package header
 * @return		available package index
 */
alKey alAddPackage(availableList al, /*@null@*/ alKey pkgKey, Header h)
	/*@modifies al, h @*/;

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
 * @return		available package pointer
 */
/*@-exportlocal@*/
/*@only@*/ /*@null@*/
alKey * alAllFileSatisfiesDepend(const availableList al, const rpmDepSet ds)
	/*@globals fileSystem @*/
	/*@modifies al, fileSystem @*/;
/*@=exportlocal@*/

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param ds		dependency set
 * @return		available package keys
 */
/*@only@*/ /*@null@*/
alKey * alAllSatisfiesDepend(const availableList al, const rpmDepSet ds)
	/*@globals fileSystem @*/
	/*@modifies al, fileSystem @*/;

/**
 * Check added package file lists for first package that has a provide.
 * @todo Eliminate.
 * @param al		available list
 * @param ds		dependency set
 * @return		available package index, -1 on not found
 */
alKey alSatisfiesDepend(const availableList al, const rpmDepSet ds)
	/*@globals fileSystem @*/
	/*@modifies al, fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
