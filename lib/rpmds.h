#ifndef H_RPMDS
#define H_RPMDS

/** \ingroup rpmdep rpmtrans
 * \file lib/rpmds.h
 * Structure used for handling a dependency set.
 */

/**
 */
typedef /*@abstract@*/ struct rpmFNSet_s *		rpmFNSet;

/**
 * A package filename set.
 */
struct rpmFNSet_s {
    int i;			/*!< File index. */

/*@observer@*/
    const char * Type;		/*!< Tag name. */

    rpmTag tagN;		/*!< Header tag. */
/*@refcounted@*/ /*@null@*/
    Header h;			/*!< Header for file name set (or NULL) */

/*@only@*/
    const char ** bnl;		/*!< Base name(s) (from header) */
/*@only@*/
    const char ** dnl;		/*!< Directory name(s) (from header) */

/*@only@*/
    const char ** fmd5s;	/*!< File MD5 sum(s) (from header) */
/*@only@*/
    const char ** flinks;	/*!< File link(s) (from header) */
/*@only@*/
    const char ** flangs;	/*!< File lang(s) */

/*@only@*/
    const uint_32 * dil;	/*!< Directory indice(s) (from header) */
/*@only@*/
    const uint_32 * fflags;	/*!< File flag(s) (from header) */
/*@only@*/
    const uint_32 * fsizes;	/*!< File size(s) (from header) */
/*@only@*/
    const uint_32 * fmtimes;	/*!< File modification time(s) (from header) */
/*@only@*/
    const uint_16 * fmodes;	/*!< File mode(s) (from header) */
/*@only@*/
    const uint_16 * frdevs;	/*!< File rdev(s) (from header) */

/*@only@*/
    char * fstates;		/*!< File state(s) (from header) */

    int_32 dc;			/*!< No. of directories. */
    int_32 fc;			/*!< No. of files. */
};

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
    int_32 Count;		/*!< No. of elements */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy a file name set.
 * @param ds		file name set
 * @return		NULL always
 */
/*@only@*/ /*@null@*/
rpmFNSet fnsFree(/*@only@*/ /*@null@*/ rpmFNSet fns)
	/*@modifies fns@*/;
/**
 * Create and load a file name set.
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES
 * @param scareMem	Use pointers to refcounted header memory?
 * @return		new file name set
 */
/*@only@*/ /*@null@*/
rpmFNSet fnsNew(Header h, rpmTag tagN, int scareMem)
	/*@modifies h @*/;

/**
 * Destroy a dependency set.
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
 * @param dspfx		formatted dependency string prefix
 * @param ds		dependency set
 * @return		new formatted dependency (malloc'ed)
 */
/*@only@*/
char * dsDNEVR(const char * dspfx, const rpmDepSet ds)
	/*@*/;

/**
 * Return dependency set count.
 * @param ds		dependency set
 * @return		current count
 */
int dsiGetCount(/*@null@*/ rpmDepSet ds)
	/*@*/;

/**
 * Return dependency set index.
 * @param ds		dependency set
 * @return		current index
 */
int dsiGetIx(/*@null@*/ rpmDepSet ds)
	/*@*/;

/**
 * Set dependency set index.
 * @param ds		dependency set
 * @param ix		new index
 * @return		current index
 */
int dsiSetIx(/*@null@*/ rpmDepSet ds, int ix)
	/*@modifies ds @*/;

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
void dsProblem(/*@null@*/ rpmProblemSet tsprobs, Header h, const rpmDepSet ds,
		/*@only@*/ /*@null@*/ const fnpyKey * suggestedKeys)
	/*@modifies tsprobs, h @*/;

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
