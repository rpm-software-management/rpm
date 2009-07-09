#ifndef H_RPMPS
#define H_RPMPS

/** \ingroup rpmps
 * \file lib/rpmps.h
 * Structures and prototypes used for an "rpmps" problem set.
 */

#include <stdio.h>
#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpmps_debug;

/** \ingroup rpmps
 * @todo Generalize filter mechanism.
 */
typedef enum rpmprobFilterFlags_e {
    RPMPROB_FILTER_NONE		= 0,
    RPMPROB_FILTER_IGNOREOS	= (1 << 0),	/*!< from --ignoreos */
    RPMPROB_FILTER_IGNOREARCH	= (1 << 1),	/*!< from --ignorearch */
    RPMPROB_FILTER_REPLACEPKG	= (1 << 2),	/*!< from --replacepkgs */
    RPMPROB_FILTER_FORCERELOCATE= (1 << 3),	/*!< from --badreloc */
    RPMPROB_FILTER_REPLACENEWFILES= (1 << 4),	/*!< from --replacefiles */
    RPMPROB_FILTER_REPLACEOLDFILES= (1 << 5),	/*!< from --replacefiles */
    RPMPROB_FILTER_OLDPACKAGE	= (1 << 6),	/*!< from --oldpackage */
    RPMPROB_FILTER_DISKSPACE	= (1 << 7),	/*!< from --ignoresize */
    RPMPROB_FILTER_DISKNODES	= (1 << 8)	/*!< from --ignoresize */
} rpmprobFilterFlags;

/**
 * Raw data for an element of a problem set.
 */
typedef struct rpmProblem_s * rpmProblem;

/** \ingroup rpmps
 * Transaction problems found while processing a transaction set/
 */
typedef struct rpmps_s * rpmps;

typedef struct rpmpsi_s * rpmpsi;

/** \ingroup rpmps
 * Enumerate transaction set problem types.
 */
typedef enum rpmProblemType_e {
    RPMPROB_BADARCH,	/*!< package ... is for a different architecture */
    RPMPROB_BADOS,	/*!< package ... is for a different operating system */
    RPMPROB_PKG_INSTALLED, /*!< package ... is already installed */
    RPMPROB_BADRELOCATE,/*!< path ... is not relocatable for package ... */
    RPMPROB_REQUIRES,	/*!< package ... has unsatisfied Requires: ... */
    RPMPROB_CONFLICT,	/*!< package ... has unsatisfied Conflicts: ... */
    RPMPROB_NEW_FILE_CONFLICT, /*!< file ... conflicts between attemped installs of ... */
    RPMPROB_FILE_CONFLICT,/*!< file ... from install of ... conflicts with file from package ... */
    RPMPROB_OLDPACKAGE,	/*!< package ... (which is newer than ...) is already installed */
    RPMPROB_DISKSPACE,	/*!< installing package ... needs ... on the ... filesystem */
    RPMPROB_DISKNODES,	/*!< installing package ... needs ... on the ... filesystem */
 } rpmProblemType;

/** \ingroup rpmps
 * Create a problem item.
 * @param type		type of problem
 * @param pkgNEVR	package name
 * @param key		filename or python object address
 * @param dn		directory name
 * @param bn		file base name
 * @param altNEVR	related (e.g. through a dependency) package name
 * @param number	generic number attribute
 * @return		rpmProblem
 */
rpmProblem rpmProblemCreate(rpmProblemType type,
                            const char * pkgNEVR,
                            fnpyKey key,
                            const char * dn, const char * bn,
                            const char * altNEVR,
                            uint64_t number);

/** \ingroup rpmps
 * Destroy a problem item.
 * @param prob		rpm problem
 * @return		rpm problem (NULL)
 */
rpmProblem rpmProblemFree(rpmProblem prob);

/** \ingroup rpmps
 * Reference an rpmProblem instance
 * @param prob		rpm problem
 * @return		rpm problem
 */
rpmProblem rpmProblemLink(rpmProblem prob);

/** \ingroup rpmps
 * Unreference an rpmProblem instance
 * @param prob		rpm problem
 * @return		rpm problem
 */
rpmProblem rpmProblemUnlink(rpmProblem prob);

/** \ingroup rpmps
 * Return package NEVR
 * @param prob		rpm problem
 * @return		package NEVR
 */
const char * rpmProblemGetPkgNEVR(const rpmProblem prob);
/** \ingroup rpmps
 * Return related (e.g. through a dependency) package NEVR
 * @param prob		rpm problem
 * @return		related (e.g. through a dependency) package NEVR
 */
const char * rpmProblemGetAltNEVR(const rpmProblem prob);

/** \ingroup rpmps
 * Return type of problem (dependency, diskpace etc)
 * @param prob		rpm problem
 * @return		type of problem
 */

rpmProblemType rpmProblemGetType(const rpmProblem prob);

/** \ingroup rpmps
 * Return filename or python object address of a problem
 * @param prob		rpm problem
 * @return		filename or python object address
 */
fnpyKey rpmProblemGetKey(const rpmProblem prob);

/** \ingroup rpmps
 * Return a generic data string from a problem
 * @param prob		rpm problem
 * @return		a generic data string
 * @todo		needs a better name
 */
const char * rpmProblemGetStr(const rpmProblem prob);

/** \ingroup rpmps
 * Return disk requirement (needed disk space / number of inodes)
 * depending on problem type. On problem types other than RPMPROB_DISKSPACE
 * and RPMPROB_DISKNODES return value is undefined.
 * @param prob		rpm problem
 * @return		disk requirement
 */
rpm_loff_t rpmProblemGetDiskNeed(const rpmProblem prob);

/** \ingroup rpmps
 * Return formatted string representation of a problem.
 * @param prob		rpm problem
 * @return		formatted string (malloc'd)
 */
char * rpmProblemString(const rpmProblem prob);

/** \ingroup rpmps
 * Unreference a problem set instance.
 * @param ps		problem set
 * @param msg
 * @return		problem set
 */
rpmps rpmpsUnlink (rpmps ps,
		const char * msg);

/** \ingroup rpmps
 * Reference a problem set instance.
 * @param ps		transaction set
 * @param msg
 * @return		new transaction set reference
 */
rpmps rpmpsLink (rpmps ps, const char * msg);

/** \ingroup rpmps
 * Return number of problems in set.
 * @param ps		problem set
 * @return		number of problems
 */
int rpmpsNumProblems(rpmps ps);

/** \ingroup rpmps
 * Initialize problem set iterator.
 * @param ps		problem set
 * @return		problem set iterator
 */
rpmpsi rpmpsInitIterator(rpmps ps);

/** \ingroup rpmps
 * Destroy problem set iterator.
 * @param psi		problem set iterator
 * @return		problem set iterator (NULL)
 */
rpmpsi rpmpsFreeIterator(rpmpsi psi);

/** \ingroup rpmps
 * Return next problem set iterator index
 * @param psi		problem set iterator
 * @return		iterator index, -1 on termination
 */
int rpmpsNextIterator(rpmpsi psi);

/** \ingroup rpmps
 * Return current problem from problem set
 * @param psi		problem set iterator
 * @return		current rpmProblem 
 */
rpmProblem rpmpsGetProblem(rpmpsi psi);

/** \ingroup rpmps
 * Create a problem set.
 * @return		new problem set
 */
rpmps rpmpsCreate(void);

/** \ingroup rpmps
 * Destroy a problem set.
 * @param ps		problem set
 * @return		NULL always
 */
rpmps rpmpsFree(rpmps ps);

/** \ingroup rpmps
 * Print problems to file handle.
 * @param fp		file handle (NULL uses stderr)
 * @param ps		problem set
 */
void rpmpsPrint(FILE *fp, rpmps ps);

/** \ingroup rpmps
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param prob		rpmProblem 
 */
void rpmpsAppendProblem(rpmps ps, rpmProblem prob);

/** \ingroup rpmps
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param type		type of problem
 * @param pkgNEVR	package name
 * @param key		filename or python object address
 * @param dn		directory name
 * @param bn		file base name
 * @param altNEVR	related (e.g. through a dependency) package name
 * @param number	generic number attribute
 */
void rpmpsAppend(rpmps ps, rpmProblemType type,
		const char * pkgNEVR,
		fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR,
		uint64_t number);

/** \ingroup rpmps
 * Filter a problem set.
 *
 * As the problem sets are generated in an order solely dependent
 * on the ordering of the packages in the transaction, and that
 * ordering can't be changed, the problem sets must be parallel to
 * one another. Additionally, the filter set must be a subset of the
 * target set, given the operations available on transaction set.
 * This is good, as it lets us perform this trim in linear time, rather
 * then logarithmic or quadratic.
 *
 * @param ps		problem set
 * @param filter	problem filter (or NULL)
 * @return		0 no problems, 1 if problems remain
 */
int rpmpsTrim(rpmps ps, rpmps filter);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMPS */
