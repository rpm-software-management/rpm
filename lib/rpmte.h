#ifndef H_RPMTE
#define H_RPMTE

/** \ingroup rpmts rpmte
 * \file lib/rpmte.h
 * Structures used for an "rpmte" transaction element.
 */

/**
 */
/*@-exportlocal@*/
/*@unchecked@*/
extern int _rpmte_debug;
/*@=exportlocal@*/

/**
 * Transaction element ordering chain linkage.
 */
typedef /*@abstract@*/ struct tsortInfo_s *		tsortInfo;

/**
 * Transaction element iterator.
 */
typedef /*@abstract@*/ struct rpmtsi_s *		rpmtsi;

/** \ingroup rpmte
 * Transaction element type.
 */
typedef enum rpmElementType_e {
    TR_ADDED		= (1 << 0),	/*!< Package will be installed. */
    TR_REMOVED		= (1 << 1)	/*!< Package will be removed. */
} rpmElementType;

#if	defined(_RPMTE_INTERNAL)
/** \ingroup rpmte
 * Dependncy ordering information.
 */
/*@-fielduse@*/	/* LCL: confused by union? */
struct tsortInfo_s {
    union {
	int	count;
	/*@exposed@*/ /*@dependent@*/ /*@null@*/
	rpmte	suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
/*@owned@*/ /*@null@*/
    struct tsortInfo_s * tsi_next;
/*@exposed@*/ /*@dependent@*/ /*@null@*/
    rpmte tsi_chain;
    int		tsi_reqx;
    int		tsi_qcnt;
};
/*@=fielduse@*/

/** \ingroup rpmte
 * A single package instance to be installed/removed atomically.
 */
struct rpmte_s {
    rpmElementType type;	/*!< Package disposition (installed/removed). */

/*@refcounted@*/ /*@relnull@*/
    Header h;			/*!< Package header. */
/*@only@*/
    const char * NEVR;		/*!< Package name-version-release. */
/*@only@*/
    const char * NEVRA;		/*!< Package name-version-release.arch. */
/*@owned@*/
    const char * name;		/*!< Name: */
/*@only@*/ /*@null@*/
    char * epoch;
/*@dependent@*/ /*@null@*/
    char * version;		/*!< Version: */
/*@dependent@*/ /*@null@*/
    char * release;		/*!< Release: */
/*@only@*/ /*@null@*/
    const char * arch;		/*!< Architecture hint. */
/*@only@*/ /*@null@*/
    const char * os;		/*!< Operating system hint. */
    int archScore;		/*!< (TR_ADDED) Arch score. */
    int osScore;		/*!< (TR_ADDED) Os score. */
    int isSource;		/*!< (TR_ADDED) source rpm? */

    rpmte parent;		/*!< Parent transaction element. */
    int degree;			/*!< No. of immediate children. */
    int npreds;			/*!< No. of predecessors. */
    int tree;			/*!< Tree index. */
    int depth;			/*!< Depth in dependency tree. */
    int breadth;		/*!< Breadth in dependency tree. */
    unsigned int db_instance;   /*!< Database Instance after add */
/*@owned@*/
    tsortInfo tsi;		/*!< Dependency ordering chains. */

/*@refcounted@*/ /*@null@*/
    rpmds this;			/*!< This package's provided NEVR. */
/*@refcounted@*/ /*@null@*/
    rpmds provides;		/*!< Provides: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmds requires;		/*!< Requires: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmds conflicts;		/*!< Conflicts: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmds obsoletes;		/*!< Obsoletes: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmfi fi;			/*!< File information. */

    uint_32 color;		/*!< Color bit(s) from package dependencies. */
    uint_32 pkgFileSize;	/*!< No. of bytes in package file (approx). */

/*@exposed@*/ /*@dependent@*/ /*@null@*/
    fnpyKey key;		/*!< (TR_ADDED) Retrieval key. */
/*@owned@*/ /*@null@*/
    rpmRelocation * relocs;	/*!< (TR_ADDED) Payload file relocations. */
    int nrelocs;		/*!< (TR_ADDED) No. of relocations. */
    int autorelocatex;		/*!< (TR_ADDED) Auto relocation entry index. */
/*@refcounted@*/ /*@null@*/	
    FD_t fd;			/*!< (TR_ADDED) Payload file descriptor. */

/*@-fielduse@*/	/* LCL: confused by union? */
    union {
/*@exposed@*/ /*@dependent@*/ /*@null@*/
	alKey addedKey;
	struct {
/*@exposed@*/ /*@dependent@*/ /*@null@*/
	    alKey dependsOnKey;
	    int dboffset;
	} removed;
    } u;
/*@=fielduse@*/

};

/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct rpmtsi_s {
/*@refcounted@*/
    rpmts ts;		/*!< transaction set. */
    int reverse;	/*!< reversed traversal? */
    int ocsave;		/*!< last returned iterator index. */
    int oc;		/*!< iterator index. */
};

#endif	/* _RPMTE_INTERNAL */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Destroy a transaction element.
 * @param te		transaction element
 * @return		NULL always
 */
/*@null@*/
rpmte rpmteFree(/*@only@*/ /*@null@*/ rpmte te)
	/*@globals fileSystem @*/
	/*@modifies te, fileSystem @*/;

/**
 * Create a transaction element.
 * @param ts		transaction set
 * @param h		header
 * @param type		TR_ADDED/TR_REMOVED
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 * @param dboffset	(TR_REMOVED) rpmdb instance
 * @param pkgKey	associated added package (if any)
 * @return		new transaction element
 */
/*@only@*/ /*@null@*/
rpmte rpmteNew(const rpmts ts, Header h, rpmElementType type,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ rpmRelocation * relocs,
		int dboffset,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey pkgKey)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, h, rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Retrieve header from transaction element.
 * @param te		transaction element
 * @return		header
 */
extern Header rpmteHeader(rpmte te)
	/*@modifies te @*/;

/**
 * Save header into transaction element.
 * @param te		transaction element
 * @param h		header
 * @return		NULL always
 */
extern Header rpmteSetHeader(rpmte te, Header h)
	/*@modifies te, h @*/;

/**
 * Retrieve type of transaction element.
 * @param te		transaction element
 * @return		type
 */
rpmElementType rpmteType(rpmte te)
	/*@*/;

/**
 * Retrieve name string of transaction element.
 * @param te		transaction element
 * @return		name string
 */
/*@observer@*/
extern const char * rpmteN(rpmte te)
	/*@*/;

/**
 * Retrieve epoch string of transaction element.
 * @param te		transaction element
 * @return		epoch string
 */
/*@observer@*/ /*@null@*/
extern const char * rpmteE(rpmte te)
	/*@*/;

/**
 * Retrieve version string of transaction element.
 * @param te		transaction element
 * @return		version string
 */
/*@observer@*/ /*@null@*/
extern const char * rpmteV(rpmte te)
	/*@*/;

/**
 * Retrieve release string of transaction element.
 * @param te		transaction element
 * @return		release string
 */
/*@observer@*/ /*@null@*/
extern const char * rpmteR(rpmte te)
	/*@*/;

/**
 * Retrieve arch string of transaction element.
 * @param te		transaction element
 * @return		arch string
 */
/*@observer@*/ /*@null@*/
extern const char * rpmteA(rpmte te)
	/*@*/;

/**
 * Retrieve os string of transaction element.
 * @param te		transaction element
 * @return		os string
 */
/*@observer@*/ /*@null@*/
extern const char * rpmteO(rpmte te)
	/*@*/;

/**
 * Retrieve isSource attribute of transaction element.
 * @param te		transaction element
 * @return		isSource attribute
 */
extern int rpmteIsSource(rpmte te)
	/*@*/;

/**
 * Retrieve color bits of transaction element.
 * @param te		transaction element
 * @return		color bits
 */
uint_32 rpmteColor(rpmte te)
	/*@*/;

/**
 * Set color bits of transaction element.
 * @param te		transaction element
 * @param color		new color bits
 * @return		previous color bits
 */
uint_32 rpmteSetColor(rpmte te, uint_32 color)
	/*@modifies te @*/;

/**
 * Retrieve last instance installed to the database.
 * @param te		transaction element
 * @return		last install instance.
 */
unsigned int rpmteDBInstance(rpmte te)
	/*@*/;

/**
 * Set last instance installed to the database.
 * @param te		transaction element
 * @param instance	Database instance of last install element.
 * @return		last install instance.
 */
void rpmteSetDBInstance(rpmte te, unsigned int instance)
	/*@modifies te @*/;

/**
 * Retrieve size in bytes of package file.
 * @todo Signature header is estimated at 256b.
 * @param te		transaction element
 * @return		size in bytes of package file.
 */
uint_32 rpmtePkgFileSize(rpmte te)
	/*@*/;

/**
 * Retrieve dependency tree depth of transaction element.
 * @param te		transaction element
 * @return		depth
 */
int rpmteDepth(rpmte te)
	/*@*/;

/**
 * Set dependency tree depth of transaction element.
 * @param te		transaction element
 * @param ndepth	new depth
 * @return		previous depth
 */
int rpmteSetDepth(rpmte te, int ndepth)
	/*@modifies te @*/;

/**
 * Retrieve dependency tree breadth of transaction element.
 * @param te		transaction element
 * @return		breadth
 */
int rpmteBreadth(rpmte te)
	/*@*/;

/**
 * Set dependency tree breadth of transaction element.
 * @param te		transaction element
 * @param nbreadth	new breadth
 * @return		previous breadth
 */
int rpmteSetBreadth(rpmte te, int nbreadth)
	/*@modifies te @*/;

/**
 * Retrieve tsort no. of predecessors of transaction element.
 * @param te		transaction element
 * @return		no. of predecessors
 */
int rpmteNpreds(rpmte te)
	/*@*/;

/**
 * Set tsort no. of predecessors of transaction element.
 * @param te		transaction element
 * @param npreds	new no. of predecessors
 * @return		previous no. of predecessors
 */
int rpmteSetNpreds(rpmte te, int npreds)
	/*@modifies te @*/;

/**
 * Retrieve tree index of transaction element.
 * @param te		transaction element
 * @return		tree index
 */
int rpmteTree(rpmte te)
	/*@*/;

/**
 * Set tree index of transaction element.
 * @param te		transaction element
 * @param ntree		new tree index
 * @return		previous tree index
 */
int rpmteSetTree(rpmte te, int ntree)
	/*@modifies te @*/;

/**
 * Retrieve parent transaction element.
 * @param te		transaction element
 * @return		parent transaction element
 */
/*@observer@*/ /*@unused@*/
rpmte rpmteParent(rpmte te)
	/*@*/;

/**
 * Set parent transaction element.
 * @param te		transaction element
 * @param pte		new parent transaction element
 * @return		previous parent transaction element
 */
/*@null@*/
rpmte rpmteSetParent(rpmte te, rpmte pte)
	/*@modifies te @*/;

/**
 * Retrieve number of children of transaction element.
 * @param te		transaction element
 * @return		tree index
 */
int rpmteDegree(rpmte te)
	/*@*/;

/**
 * Set number of children of transaction element.
 * @param te		transaction element
 * @param ndegree	new number of children
 * @return		previous number of children
 */
int rpmteSetDegree(rpmte te, int ndegree)
	/*@modifies te @*/;

/**
 * Retrieve tsort info for transaction element.
 * @param te		transaction element
 * @return		tsort info
 */
tsortInfo rpmteTSI(rpmte te)
	/*@*/;

/**
 * Destroy tsort info of transaction element.
 * @param te		transaction element
 */
void rpmteFreeTSI(rpmte te)
	/*@modifies te @*/;

/**
 * Initialize tsort info of transaction element.
 * @param te		transaction element
 */
void rpmteNewTSI(rpmte te)
	/*@modifies te @*/;

/**
 * Destroy dependency set info of transaction element.
 * @param te		transaction element
 */
/*@unused@*/
void rpmteCleanDS(rpmte te)
	/*@modifies te @*/;

/**
 * Retrieve pkgKey of TR_ADDED transaction element.
 * @param te		transaction element
 * @return		pkgKey
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
alKey rpmteAddedKey(rpmte te)
	/*@*/;

/**
 * Set pkgKey of TR_ADDED transaction element.
 * @param te		transaction element
 * @param npkgKey	new pkgKey
 * @return		previous pkgKey
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
alKey rpmteSetAddedKey(rpmte te,
		/*@exposed@*/ /*@dependent@*/ /*@null@*/ alKey npkgKey)
	/*@modifies te @*/;

/**
 * Retrieve dependent pkgKey of TR_REMOVED transaction element.
 * @param te		transaction element
 * @return		dependent pkgKey
 */
/*@exposed@*/ /*@dependent@*/ /*@null@*/
alKey rpmteDependsOnKey(rpmte te)
	/*@*/;

/**
 * Retrieve rpmdb instance of TR_REMOVED transaction element.
 * @param te		transaction element
 * @return		rpmdb instance
 */
int rpmteDBOffset(rpmte te)
	/*@*/;

/**
 * Retrieve name-version-release string from transaction element.
 * @param te		transaction element
 * @return		name-version-release string
 */
/*@observer@*/
extern const char * rpmteNEVR(rpmte te)
	/*@*/;

/**
 * Retrieve name-version-release.arch string from transaction element.
 * @param te		transaction element
 * @return		name-version-release.arch string
 */
/*@-exportlocal@*/
/*@observer@*/
extern const char * rpmteNEVRA(rpmte te)
	/*@*/;
/*@=exportlocal@*/

/**
 * Retrieve file handle from transaction element.
 * @param te		transaction element
 * @return		file handle
 */
FD_t rpmteFd(rpmte te)
	/*@*/;

/**
 * Retrieve key from transaction element.
 * @param te		transaction element
 * @return		key
 */
/*@exposed@*/
fnpyKey rpmteKey(rpmte te)
	/*@*/;

/**
 * Retrieve dependency tag set from transaction element.
 * @param te		transaction element
 * @param tag		dependency tag
 * @return		dependency tag set
 */
rpmds rpmteDS(rpmte te, rpmTag tag)
	/*@*/;

/**
 * Retrieve file info tag set from transaction element.
 * @param te		transaction element
 * @param tag		file info tag (RPMTAG_BASENAMES)
 * @return		file info tag set
 */
rpmfi rpmteFI(rpmte te, rpmTag tag)
	/*@*/;

/**
 * Calculate transaction element dependency colors/refs from file info.
 * @param te		transaction element
 * @param tag		dependency tag (RPMTAG_PROVIDENAME, RPMTAG_REQUIRENAME)
 */
/*@-exportlocal@*/
void rpmteColorDS(rpmte te, rpmTag tag)
        /*@modifies te @*/;
/*@=exportlocal@*/

/**
 * Return transaction element index.
 * @param tsi		transaction element iterator
 * @return		transaction element index
 */
int rpmtsiOc(rpmtsi tsi)
	/*@*/;

/**
 * Destroy transaction element iterator.
 * @param tsi		transaction element iterator
 * @return		NULL always
 */
/*@unused@*/ /*@null@*/
rpmtsi rpmtsiFree(/*@only@*//*@null@*/ rpmtsi tsi)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;

/**
 * Destroy transaction element iterator.
 * @param tsi		transaction element iterator
 * @param fn
 * @param ln
 * @return		NULL always
 */
/*@null@*/
rpmtsi XrpmtsiFree(/*@only@*//*@null@*/ rpmtsi tsi,
		const char * fn, unsigned int ln)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/;
#define	rpmtsiFree(_tsi)	XrpmtsiFree(_tsi, __FILE__, __LINE__)

/**
 * Create transaction element iterator.
 * @param ts		transaction set
 * @return		transaction element iterator
 */
/*@unused@*/ /*@only@*/
rpmtsi rpmtsiInit(rpmts ts)
	/*@modifies ts @*/;

/**
 * Create transaction element iterator.
 * @param ts		transaction set
 * @param fn
 * @param ln
 * @return		transaction element iterator
 */
/*@unused@*/ /*@only@*/
rpmtsi XrpmtsiInit(rpmts ts,
		const char * fn, unsigned int ln)
	/*@modifies ts @*/;
#define	rpmtsiInit(_ts)		XrpmtsiInit(_ts, __FILE__, __LINE__)

/**
 * Return next transaction element of type.
 * @param tsi		transaction element iterator
 * @param type		transaction element type selector (0 for any)
 * @return		next transaction element of type, NULL on termination
 */
/*@dependent@*/ /*@null@*/
rpmte rpmtsiNext(rpmtsi tsi, rpmElementType type)
        /*@modifies tsi @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMTE */
