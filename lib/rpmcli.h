#ifndef H_RPMCLI
#define	H_RPMCLI

/** \ingroup rpmcli rpmbuild
 * \file lib/rpmcli.h
 */

#include <rpmlib.h>

/** \ingroup rpmcli
 * Should version 3 packages be produced?
 */
extern int _noDirTokens;

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/** \name RPMBT */
/*@{*/

/** \ingroup rpmcli
 * Describe build command line request.
 */
struct rpmBuildArguments_s {
    int buildAmount;		/*!< Bit(s) to control operation. */
/*@null@*/ const char * buildRootOverride; /*!< from --buildroot */
/*@null@*/ char * targets;	/*!< Target platform(s), comma separated. */
    int force;			/*!< from --force */
    int noBuild;		/*!< from --nobuild */
    int noDeps;			/*!< from --nodeps */
    int noLang;			/*!< from --nolang */
    int shortCircuit;		/*!< from --short-circuit */
    int sign;			/*!< from --sign */
    int useCatalog;		/*!< from --usecatalog */
    char buildMode;		/*!< Build mode (one of "btBC") */
    char buildChar;		/*!< Build stage (one of "abcilps ") */
/*@observer@*/ /*@null@*/ const char * rootdir;
};

/** \ingroup rpmcli
 */
typedef	struct rpmBuildArguments_s *	BTA_t;

/** \ingroup rpmcli
 */
extern struct rpmBuildArguments_s	rpmBTArgs;

/** \ingroup rpmcli
 */
extern struct poptOption		rpmBuildPoptTable[];

/*@}*/
/* ==================================================================== */
/** \name RPMQV */
/*@{*/

/** \ingroup rpmcli
 * Bit(s) to control rpmQuery() operation, stored in qva_flags.
 */
/*@-typeuse@*/
typedef enum rpmQueryFlags_e {
    QUERY_FOR_LIST	= (1 << 1),	/*!< from --list */
    QUERY_FOR_STATE	= (1 << 2),	/*!< from --state */
    QUERY_FOR_DOCS	= (1 << 3),	/*!< from --docfiles */
    QUERY_FOR_CONFIG	= (1 << 4),	/*!< from --configfiles */
    QUERY_FOR_DUMPFILES	= (1 << 8)	/*!< from --dump */
} rpmQueryFlags;
/*@=typeuse@*/

/** \ingroup rpmcli
 * Bit(s) to control rpmVerify() operation, stored in qva_flags.
 */
/*@-typeuse@*/
typedef enum rpmVerifyFlags_e {
    VERIFY_FILES	= (1 <<  9),	/*!< from --nofiles */
    VERIFY_DEPS		= (1 << 10),	/*!< from --nodeps */
    VERIFY_SCRIPT	= (1 << 11),	/*!< from --noscripts */
    VERIFY_MD5		= (1 << 12)	/*!< from --nomd5 */
} rpmVerifyFlags;
/*@=typeuse@*/

/** \ingroup rpmcli
 * Describe query/verify command line request.
 */
struct rpmQVArguments_s {
    rpmQVSources qva_source;	/*!< Identify CLI arg type. */
    int 	qva_sourceCount;/*!< Exclusive check (>1 is error). */
    int		qva_flags;	/*!< Bit(s) to control operation. */
/*@unused@*/ int qva_verbose;	/*!< (unused) */
/*@only@*/ /*@null@*/ const char * qva_queryFormat; /*!< Format for headerSprintf(). */
/*@observer@*/ /*@null@*/ const char * qva_prefix; /*!< Path to top of install tree. */
    char	qva_mode;	/*!< 'q' is query, 'v' is verify mode. */
    char	qva_char;	/*!< (unused) always ' ' */
};

/** \ingroup rpmcli
 */
extern struct rpmQVArguments_s rpmQVArgs;

/** \ingroup rpmcli
 */
extern struct poptOption rpmQVSourcePoptTable[];

/** \ingroup rpmcli
 */
extern int specedit;

/** \ingroup rpmcli
 */
extern struct poptOption rpmQueryPoptTable[];

/** \ingroup rpmcli
 */
extern struct poptOption rpmVerifyPoptTable[];

/*@}*/
/* ==================================================================== */
/** \name RPMEIU */
/*@{*/
/* --- install/upgrade/erase modes */

/** \ingroup rpmcli
 * Describe database command line requests.
 */
struct rpmInstallArguments_s {
    rpmtransFlags transFlags;
    rpmprobFilterFlags probFilter;
    rpmInstallInterfaceFlags installInterfaceFlags;
    rpmEraseInterfaceFlags eraseInterfaceFlags;
/*@only@*/ rpmRelocation * relocations;
    int numRelocations;
    int noDeps;
    int force;
    int incldocs;
    const char * prefix;
};

/** \ingroup rpmcli
 */
extern struct rpmInstallArguments_s rpmIArgs;

/** \ingroup rpmcli
 */
extern struct poptOption rpmInstallPoptTable[];

/*@}*/
/* ==================================================================== */
/** \name RPMDB */
/*@{*/
/* --- database modes */

/** \ingroup rpmcli
 * Describe database command line requests.
 */
struct rpmDatabaseArguments_s {
    int init;			/*!< from --initdb */
    int rebuild;		/*!< from --rebuilddb */
    int verify;			/*!< from --verifydb */
};

/** \ingroup rpmcli
 */
extern struct rpmDatabaseArguments_s rpmDBArgs;

/** \ingroup rpmcli
 */
extern struct poptOption rpmDatabasePoptTable[];

/*@}*/
/* ==================================================================== */
/** \name RPMK */
/*@{*/

/** \ingroup rpmcli
 * Describe signature command line request.
 */
struct rpmSignArguments_s {
    rpmResignFlags addSign;	/*!< from --checksig/--resign/--addsign */
    rpmCheckSigFlags checksigFlags;	/*!< bits to control --checksig */
    int sign;			/*!< Is a passphrase needed? */
/*@unused@*/ char * passPhrase;
};

/** \ingroup rpmcli
 */
extern struct rpmSignArguments_s rpmKArgs;

/** \ingroup rpmcli
 */
extern struct poptOption rpmSignPoptTable[];

/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMCLI */
