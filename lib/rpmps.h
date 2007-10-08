#ifndef H_RPMPS
#define H_RPMPS

/** \ingroup rpmps
 * \file lib/rpmps.h
 * Structures and prototypes used for an "rpmps" problem set.
 */

#include "rpmmessages.h"	/* for fnpyKey */

#ifdef __cplusplus
extern "C" {
#endif

extern int _rpmps_debug;

/**
 * Raw data for an element of a problem set.
 */
typedef struct rpmProblem_s * rpmProblem;

/**
 * Transaction problems found while processing a transaction set/
 */
typedef struct rpmps_s * rpmps;

/**
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
    RPMPROB_BADPRETRANS	/*!< (unimplemented) */
 } rpmProblemType;

/**
 */
struct rpmProblem_s {
    char * pkgNEVR;
    char * altNEVR;
    fnpyKey key;
    rpmProblemType type;
    int ignoreProblem;
    char * str1;
    unsigned long ulong1;
};

/**
 */
struct rpmps_s {
    int numProblems;		/*!< Current probs array size. */
    int numProblemsAlloced;	/*!< Allocated probs array size. */
    rpmProblem probs;		/*!< Array of specific problems. */
    int nrefs;			/*!< Reference count. */
};

/**
 * Return formatted string representation of a problem.
 * @param prob		rpm problem
 * @return		formatted string (malloc'd)
 */
extern const char * rpmProblemString(const rpmProblem prob);

/**
 * Unreference a problem set instance.
 * @param ps		problem set
 * @param msg
 * @return		problem set
 */
rpmps rpmpsUnlink (rpmps ps,
		const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmps XrpmpsUnlink (rpmps ps,
		const char * msg, const char * fn, unsigned ln);
#define	rpmpsUnlink(_ps, _msg)	XrpmpsUnlink(_ps, _msg, __FILE__, __LINE__)

/**
 * Reference a problem set instance.
 * @param ps		transaction set
 * @param msg
 * @return		new transaction set reference
 */
rpmps rpmpsLink (rpmps ps, const char * msg);

/** @todo Remove debugging entry from the ABI. */
rpmps XrpmpsLink (rpmps ps,
		const char * msg, const char * fn, unsigned ln);
#define	rpmpsLink(_ps, _msg)	XrpmpsLink(_ps, _msg, __FILE__, __LINE__)

/**
 * Return number of problems in set.
 * @param ps		problem set
 * @return		number of problems
 */
int rpmpsNumProblems(rpmps ps);

/**
 * Create a problem set.
 * @return		new problem set
 */
rpmps rpmpsCreate(void);

/**
 * Destroy a problem set.
 * @param ps		problem set
 * @return		NULL always
 */
rpmps rpmpsFree(rpmps ps);

/**
 * Print problems to file handle.
 * @param fp		file handle (NULL uses stderr)
 * @param ps		problem set
 */
void rpmpsPrint(FILE *fp, rpmps ps);

/**
 * Append a problem to current set of problems.
 * @param ps		problem set
 * @param type		type of problem
 * @param pkgNEVR	package name
 * @param key		filename or python object address
 * @param dn		directory name
 * @param bn		file base name
 * @param altNEVR	related (e.g. through a dependency) package name
 * @param ulong1	generic pointer/long attribute
 */
void rpmpsAppend(rpmps ps, rpmProblemType type,
		const char * pkgNEVR,
		fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR,
		unsigned long ulong1);

/**
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
