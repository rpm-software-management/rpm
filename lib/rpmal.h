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

#ifdef	DYING
/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
/*@refcounted@*/
    Header h;			/*!< Package header. */
/*@dependent@*/
    const char * name;		/*!< Header name. */
/*@dependent@*/
    const char * version;	/*!< Header version. */
/*@dependent@*/
    const char * release;	/*!< Header release. */
/*@owned@*/ /*@null@*/
    rpmDepSet provides;		/*!< Provides: dependencies. */
/*@owned@*/ /*@null@*/
    rpmDepSet requires;		/*!< Requires: dependencies. */
/*@owned@*//*@null@*/
    const char ** baseNames;	/*!< Header file basenames. */
/*@dependent@*//*@null@*/
    int_32 * epoch;		/*!< Header epoch (if any). */
    int filesCount;		/*!< No. of files in header. */
#ifdef	DYING
    uint_32 multiLib;	/* MULTILIB */
#endif
/*@kept@*//*@null@*/
    const void * key;		/*!< Private data associated with a package (e.g. file name of package). */
/*@null@*/ rpmRelocation * relocs;
/*@null@*/ FD_t fd;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return number of packages in list.
 * @param al		available list
 * @return		no. of packages in list
 */
int alGetSize(const availableList al)
	/*@*/;

/**
 * Return available package identifier key.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available identifier key
 */
/*@kept@*/ /*@null@*/
const void * alGetKey(/*@null@*/ const availableList al, /*@null@*/alKey pkgKey)
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

/**
 * Return available package files count.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package files count
 */
int alGetFilesCount(/*@null@*/ const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;

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
 * Return available package relocations.
 * @warning alp->relocs set to NULL after call.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package relocations
 */
/*@null@*/
rpmRelocation * alGetRelocs(/*@null@*/ availableList al, /*@null@*/ alKey pkgKey)
	/*@modifies al @*/;

/**
 * Return available package file handle.
 * @warning alp->fd set to NULL after call.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package file handle
 */
/*@null@*/
FD_t alGetFd(/*@null@*/ availableList al, /*@null@*/ alKey pkgKey)
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
 * Return available package index.
 * @param al		available list
 * @param alp		available package pointer
 * @return		available package index, -1 on failure
 */
alNum alGetPkgIndex(/*@null@*/ const availableList al, const availablePackage alp)
	/*@*/;
#endif

/**
 * Return (malloc'd) available package name-version-release string.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		name-version-release string
 */
/*@only@*/ /*@null@*/
char * alGetNVR(/*@null@*/const availableList al, /*@null@*/ alKey pkgKey)
	/*@*/;

#ifdef	DYING
/**
 * Append available package problem to set.
 */
/*@unused@*/
void alProblemSetAppend(const availableList al, const availablePackage alp,
		rpmProblemSet tsprobs, rpmProblemType type,
		const char * dn, const char * bn,
		/*@only@*/ /*@null@*/ const char * altNEVR,
		unsigned long ulong1)
	/*@modifies tsprobs @*/;
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
 * @param h		package header
 * @param key		package private data
 * @param fd		package file handle
 * @param relocs	package file relocations
 * @return		available package index
 */
alKey alAddPackage(availableList al, /*@null@*/ alKey pkgKey,
		Header h, /*@null@*/ /*@dependent@*/ const void * key,
		/*@null@*/ FD_t fd, /*@null@*/ rpmRelocation * relocs)
	/*@modifies al, h @*/;

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
	/*@modifies fileSystem @*/;
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
	/*@modifies fileSystem @*/;

/**
 * Check added package file lists for first package that has a provide.
 * @todo Eliminate.
 * @param al		available list
 * @param ds		dependency set
 * @return		available package index, -1 on not found
 */
alKey alSatisfiesDepend(const availableList al, const rpmDepSet ds)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
