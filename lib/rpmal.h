#ifndef H_RPMAL
#define H_RPMAL

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmal.h
 * Structures used for managing added/available package lists.
 */

/**
 */
typedef /*@abstract@*/ struct tsortInfo_s *		tsortInfo;

/** \ingroup rpmdep
 * Dependncy ordering information.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct tsortInfo_s {
    union {
	int	count;
	/*@kept@*/ /*@null@*/ availablePackage suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/
    struct tsortInfo_s * tsi_next;
/*@kept@*/ /*@null@*/
    availablePackage tsi_pkg;
    int		tsi_reqx;
    int		tsi_qcnt;
};
/*@=fielduse@*/

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
    Header h;				/*!< Package header. */
/*@dependent@*/ const char * name;	/*!< Header name. */
/*@dependent@*/ const char * version;	/*!< Header version. */
/*@dependent@*/ const char * release;	/*!< Header release. */
/*@owned@*/ const char ** provides;	/*!< Provides: name strings. */
/*@owned@*/ const char ** providesEVR;	/*!< Provides: [epoch:]version[-release] strings. */
/*@dependent@*/ int * provideFlags;	/*!< Provides: logical range qualifiers. */
/*@owned@*//*@null@*/ const char ** requires;	/*!< Requires: name strings. */
/*@owned@*//*@null@*/ const char ** requiresEVR;/*!< Requires: [epoch:]version[-release] strings. */
/*@dependent@*//*@null@*/ int * requireFlags;	/*!< Requires: logical range qualifiers. */
/*@owned@*//*@null@*/ const char ** baseNames;	/*!< Header file basenames. */
/*@dependent@*//*@null@*/ int_32 * epoch;	/*!< Header epoch (if any). */
    int providesCount;			/*!< No. of Provide:'s in header. */
    int requiresCount;			/*!< No. of Require:'s in header. */
    int filesCount;			/*!< No. of files in header. */
    int npreds;				/*!< No. of predecessors. */
    int depth;				/*!< Max. depth in dependency tree. */
    struct tsortInfo_s tsi;		/*!< Dependency tsort data. */
    uint_32 multiLib;	/* MULTILIB */
/*@kept@*//*@null@*/ const void * key;	/*!< Private data associated with a package (e.g. file name of package). */
/*@null@*/ rpmRelocation * relocs;
/*@null@*/ FD_t fd;
};

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
 * Return available package key.
 * @param al		available list
 * @param pkgNum	available package index
 * @return		available package key
 */
/*@kept@*/ /*@null@*/
const void * alGetKey(/*@null@*/ const availableList al, int pkgNum)
	/*@*/;

/**
 * Return available package multiLib flag.
 * @param al		available list
 * @param pkgNum	available package index
 * @return		available package multiLib flag
 */
int alGetMultiLib(/*@null@*/ const availableList al, int pkgNum)
	/*@*/;

/**
 * Return available package files count.
 * @param al		available list
 * @param pkgNum	available package index
 * @return		available package files count
 */
int alGetFilesCount(/*@null@*/ const availableList al, int pkgNum)
	/*@*/;

/**
 * Return available package header.
 * @param al		available list
 * @param pkgNum	available package index
 * @param unlink	Should alp->h be unlinked?
 * @return		available package header
 */
Header alGetHeader(/*@null@*/ availableList al, int pkgNum, int unlink)
	/*@modifies al @*/;

/**
 * Return available package relocations.
 * @warning alp->relocs set to NULL after call.
 * @param al		available list
 * @param pkgNum	available package index
 * @return		available package relocations
 */
/*@null@*/
rpmRelocation * alGetRelocs(/*@null@*/ availableList al, int pkgNum)
	/*@modifies al @*/;

/**
 * Return available package file handle.
 * @warning alp->fd set to NULL after call.
 * @param al		available list
 * @param pkgNum	available package index
 * @return		available package file handle
 */
/*@null@*/
FD_t alGetFd(/*@null@*/ availableList al, int pkgNum)
	/*@modifies al @*/;

/**
 * Return available package.
 * @param al		available list
 * @param pkgNum	available package index
 * @return		available package pointer
 */
/*@dependent@*/ /*@null@*/
availablePackage alGetPkg(/*@null@*/ availableList al, int pkgNum)
	/*@*/;

/**
 * Return available package index.
 * @param al		available list
 * @param alp		available package pointer
 * @return		available package index, -1 on failure
 */
int alGetPkgIndex(/*@null@*/ const availableList al, const availablePackage alp)
	/*@*/;

/**
 * Return (malloc'd) available package name-version-release string.
 * @param al		available list
 * @param alp		available package pointer
 * @return		name-version-release string
 */
/*@-exportlocal@*/
/*@only@*/ /*@null@*/
char * alGetPkgNVR(/*@null@*/const availableList al, const availablePackage alp)
	/*@*/;
/*@=exportlocal@*/

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
 * @param pkgNnum	package index
 */
/*@-exportlocal@*/
void alDelPackage(availableList al, int pkgNum)
	/*@modifies al @*/;
/*@=exportlocal@*/

/**
 * Add package to available list.
 * @param al		available list
 * @param pkgNnum	package index, < 0 to force an append
 * @param h		package header
 * @param key		package private data
 * @param fd		package file handle
 * @param relocs	package file relocations
 * @return		available package pointer
 */
/*@exposed@*/
availablePackage alAddPackage(availableList al, int pkgNum,
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
 * @param keyType	type of dependency
 * @param fileName	file name to search for
 * @return		available package pointer
 */
/*@-exportlocal@*/
/*@only@*/ /*@null@*/
availablePackage * alAllFileSatisfiesDepend(const availableList al,
		const char * keyType, const char * fileName)
	/*@*/;
/*@=exportlocal@*/

/**
 * Check added package file lists for package(s) that have a provide.
 * @param al		available list
 * @param keyType	type of dependency
 * @param keyDepend	dependency string representation
 * @param keyName	dependency name string
 * @param keyEVR	dependency [epoch:]version[-release] string
 * @param keyFlags	dependency logical range qualifiers
 * @return		available package pointer
 */
/*@only@*/ /*@null@*/
availablePackage * alAllSatisfiesDepend(const availableList al,
		const char * keyType, const char * keyDepend,
		const char * keyName, const char * keyEVR, int keyFlags)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMAL */
