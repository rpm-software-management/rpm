#ifndef H_RPMDS
#define H_RPMDS

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmds.h
 * Structure used for handling a dependency set.
 */

/**
 */
typedef /*@abstract@*/ struct problemsSet_s *		problemsSet;

/**
 * Problems encountered while checking dependencies.
 */
struct problemsSet_s {
    rpmDependencyConflict problems;	/*!< Problems encountered. */
    int num;			/*!< No. of problems found. */
    int alloced;		/*!< No. of problems allocated. */
} ;

/**
 * A package dependency set.
 */
struct rpmDepSet_s {
    int i;			/*!< Element index. */

/*@observer@*/
    const char * Type;		/*!< Tag name. */
/*@only@*/ /*@null@*/
    const char * DNEVR;		/*!< Formatted dependency string. */

    rpmTag tagN;		/*!< Header tag. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Header for dependency set (or NULL) */
/*@only@*/
    const char ** N;		/*!< Name. */
/*@only@*/
    const char ** EVR;		/*!< Epoch-Version-Release. */
/*@only@*/
    const int_32 * Flags;	/*!< Flags identifying context/comparison. */
    rpmTagType Nt, EVRt, Ft;	/*!< Tag data types. */
    int Count;			/*!< No. of elements */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy a new dependency set.
 * @param ds		dependency set
 * @return		NULL always
 */
/*@only@*/ /*@null@*/
rpmDepSet dsFree(/*@only@*/ /*@null@*/ rpmDepSet ds)
	/*@modifies ds@*/;
/**
 * Create and load a dependency set.
 * @param h		header
 * @param tagN		type of dependency
 * @param scareMem	Use pointers to refcounted header memory?
 * @return		new dependency set
 */
/*@only@*/ /*@null@*/
rpmDepSet dsNew(Header h, rpmTag tagN, int scareMem)
	/*@modifies h @*/;

/**
 * Return new formatted dependency string.
 * @param depend	type of dependency ("R" == Requires, "C" == Conflcts)
 * @param key		dependency
 * @return		new formatted dependency (malloc'ed)
 */
/*@only@*/
char * dsDNEVR(const char * depend, const rpmDepSet key)
	/*@*/;

/**
 * Return current formatted dependency string.
 * @param ds		dependency set
 * @return		current dependency DNEVR, NULL on invalid
 */
/*@null@*/
const char * dsiGetDNEVR(/*@null@*/ rpmDepSet ds)
	/*@*/;

/**
 * Return current dependency name.
 * @param ds		dependency set
 * @return		current dependency name, NULL on invalid
 */
/*@null@*/
const char * dsiGetN(/*@null@*/ rpmDepSet ds)
	/*@*/;

/**
 * Return current dependency epoch-version-release.
 * @param ds		dependency set
 * @return		current dependency EVR, NULL on invalid
 */
/*@null@*/
const char * dsiGetEVR(/*@null@*/ rpmDepSet ds)
	/*@*/;

/**
 * Return current dependency Flags.
 * @param ds		dependency set
 * @return		current dependency EVR, 0 on invalid
 */
int_32 dsiGetFlags(/*@null@*/ rpmDepSet ds)
	/*@*/;

/**
 * Notify of results of dependency match;
 * @param ds		dependency set
 * @param where		where dependency was resolved (or NULL)
 * @param rc		0 == YES, otherwise NO
 */
/*@-globuse@*/ /* FIX: rpmMessage annotation is a lie */
void dsiNotify(/*@null@*/ rpmDepSet ds, /*@null@*/ const char * where, int rc)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
/*@=globuse@*/

/**
 * Return next dependency set iterator index.
 * @param ds		dependency set
 * @return		dependency set iterator index, -1 on termination
 */
int dsiNext(/*@null@*/ rpmDepSet ds)
	/*@modifies ds @*/;

/**
 * Initialize dependency set iterator.
 * @param ds		dependency set
 * @return		dependency set
 */
/*@null@*/
rpmDepSet dsiInit(/*@returned@*/ /*@null@*/ rpmDepSet ds)
	/*@modifies ds @*/;

/**
 * Compare two versioned dependency ranges, looking for overlap.
 * @param A		1st dependency
 * @param B		2nd dependency
 * @return		1 if dependencies overlap, 0 otherwise
 */
int dsCompare(const rpmDepSet A, const rpmDepSet B)
	/*@*/;

/**
 * Report a Requires: or Conflicts: dependency problem.
 */
void dsProblem(problemsSet psp, Header h, const rpmDepSet dep,
		/*@only@*/ /*@null@*/ const void ** suggestedPackages)
	/*@modifies psp, h @*/;

/**
 * Compare package provides dependencies from header with a single dependency.
 * @param h		header
 * @param ds		dependency set
 */
int rangeMatchesDepFlags (Header h, const rpmDepSet req)
        /*@modifies h @*/;

/**
 * Compare package name-version-release from header with a single dependency.
 * @deprecated Remove from API when obsoletes is correctly implemented.
 * @param h		header
 * @param req		dependency
 * @return		1 if dependency overlaps, 0 otherwise
 */
int headerMatchesDepFlags(Header h, const rpmDepSet req)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMDS */
