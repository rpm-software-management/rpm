#ifndef H_RPMPS
#define H_RPMPS

/** \ingroup rpmps
 * \file lib/rpmps.h
 * Structures and prototypes used for an "rpmps" problem set.
 */

/**
 * Raw data for an element of a problem set.
 */
typedef /*@abstract@*/ struct rpmProblem_s * rpmProblem;

/**
 * Transaction problems found while processing a transaction set/
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct rpmps_s * rpmps;

/**
 * Enumerate transaction set problem types.
 */
typedef enum rpmProblemType_e {
    RPMPROB_BADARCH,	/*!< package ... is for a different architecture */
    RPMPROB_BADOS,	/*!< package ... is for a different operating system */
    RPMPROB_PKG_INSTALLED, /*!< package ... is already installed */
    RPMPROB_BADRELOCATE,/*!< path ... is not relocateable for package ... */
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
/*@only@*/ /*@null@*/
    char * pkgNEVR;
/*@only@*/ /*@null@*/
    char * altNEVR;
/*@exposed@*/ /*@null@*/
    fnpyKey key;
    rpmProblemType type;
    int ignoreProblem;
/*@only@*/ /*@null@*/
    char * str1;
    unsigned long ulong1;
};

/**
 */
struct rpmps_s {
    int numProblems;		/*!< Current probs array size. */
    int numProblemsAlloced;	/*!< Allocated probs array size. */
    rpmProblem probs;		/*!< Array of specific problems. */
/*@refs@*/
    int nrefs;			/*!< Reference count. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
void printDepFlags(FILE *fp, const char *version, int flags)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 * Print a problem array.
 * @param fp		output file
 * @param ps		dependency problems
 */
void printDepProblems(FILE * fp, /*@null@*/ const rpmps ps)
	/*@globals fileSystem @*/
	/*@modifies *fp, fileSystem @*/;

/**
 * Return formatted string representation of a problem.
 * @param prob		rpm problem
 * @return		formatted string (malloc'd)
 */
/*@-redecl@*/	/* LCL: is confused. */
/*@only@*/ extern const char * rpmProblemString(const rpmProblem prob)
	/*@*/;
/*@=redecl@*/

/**
 * Unreference a problem set instance.
 * @param ps		problem set
 * @param msg
 * @return		problem set
 */
/*@unused@*/
rpmps rpmpsUnlink (/*@killref@*/ /*@returned@*/ rpmps ps,
		const char * msg)
	/*@modifies ps @*/;

/** @todo Remove debugging entry from the ABI. */
/*@null@*/
rpmps XrpmpsUnlink (/*@killref@*/ /*@returned@*/ rpmps ps,
		const char * msg, const char * fn, unsigned ln)
	/*@modifies ps @*/;
#define	rpmpsUnlink(_ps, _msg)	XrpmpsUnlink(_ps, _msg, __FILE__, __LINE__)

/**
 * Reference a problem set instance.
 * @param ps		transaction set
 * @param msg
 * @return		new transaction set reference
 */
/*@unused@*/
rpmps rpmpsLink (rpmps ps, const char * msg)
	/*@modifies ps @*/;

/** @todo Remove debugging entry from the ABI. */
rpmps XrpmpsLink (rpmps ps,
		const char * msg, const char * fn, unsigned ln)
        /*@modifies ps @*/;
#define	rpmpsLink(_ps, _msg)	XrpmpsLink(_ps, _msg, __FILE__, __LINE__)

/**
 * Create a problem set.
 */
rpmps rpmpsCreate(void)
	/*@*/;

/**
 * Destroy a problem set.
 * @param ps		problem set
 * @return		NULL always
 */
/*@null@*/
rpmps rpmpsFree(/*@killref@*/ /*@only@*/ /*@null@*/ rpmps ps)
	/*@modifies ps @*/;

/**
 * Output formatted string representation of a problem to file handle.
 * @deprecated API: prob used to be passed by value, now passed by reference.
 * @param fp		file handle
 * @param prob		rpm problem
 */
void rpmProblemPrint(FILE *fp, rpmProblem prob)
	/*@globals fileSystem @*/
	/*@modifies prob, *fp, fileSystem @*/;

/**
 * Print problems to file handle.
 * @param fp		file handle
 * @param ps		problem set
 */
void rpmpsPrint(FILE *fp, /*@null@*/ rpmps ps)
	/*@globals fileSystem @*/
	/*@modifies *fp, ps, fileSystem @*/;

/**
 * Append a problem to set.
 */
void rpmpsAppend(/*@null@*/ rpmps ps, rpmProblemType type,
		/*@null@*/ const char * pkgNEVR,
		/*@exposed@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ const char * dn, /*@null@*/ const char * bn,
		/*@null@*/ const char * altNEVR,
		unsigned long ulong1)
	/*@modifies ps @*/;

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
int rpmpsTrim(/*@null@*/ rpmps ps, /*@null@*/ rpmps filter)
	/*@modifies ps @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMPS */
