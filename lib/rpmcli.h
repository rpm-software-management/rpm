#ifndef H_RPMCLI
#define	H_RPMCLI

/** \ingroup rpmcli rpmbuild
 * \file lib/rpmcli.h
 */

#include "rpmlib.h"
#include "rpmurl.h"
#include "rpmmacro.h"

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
 * @todo Merge rpmQueryFlags, rpmVerifyFlags, and rpmVerifyAttrs?.
 */
typedef enum rpmQueryFlags_e {
    QUERY_FOR_DEFAULT	= 0,		/*!< */
/*@-enummemuse@*/
    QUERY_MD5		= (1 << 0),	/*!< from --nomd5 */
    QUERY_SIZE		= (1 << 1),	/*!< from --nosize */
    QUERY_LINKTO	= (1 << 2),	/*!< from --nolink */
    QUERY_USER		= (1 << 3),	/*!< from --nouser) */
    QUERY_GROUP		= (1 << 4),	/*!< from --nogroup) */
    QUERY_MTIME		= (1 << 5),	/*!< from --nomtime) */
    QUERY_MODE		= (1 << 6),	/*!< from --nomode) */
    QUERY_RDEV		= (1 << 7),	/*!< from --nodev */
	/* bits 8-15 unused, reserved for rpmVerifyAttrs */
    QUERY_FILES		= (1 << 16),	/*!< verify: from --nofiles */
    QUERY_DEPS		= (1 << 17),	/*!< verify: from --nodeps */
    QUERY_SCRIPT	= (1 << 18),	/*!< verify: from --noscripts */
    QUERY_DIGEST	= (1 << 19),	/*!< verify: from --nodigest */
/*@=enummemuse@*/
	/* bits 20-22 unused */
    QUERY_FOR_LIST	= (1 << 23),	/*!< query:  from --list */
    QUERY_FOR_STATE	= (1 << 24),	/*!< query:  from --state */
    QUERY_FOR_DOCS	= (1 << 25),	/*!< query:  from --docfiles */
    QUERY_FOR_CONFIG	= (1 << 26),	/*!< query:  from --configfiles */
    QUERY_FOR_DUMPFILES	= (1 << 27),	/*!< query:  from --dump */
} rpmQueryFlags;

/** \ingroup rpmcli
 * Bit(s) to control rpmVerify() operation, stored in qva_flags.
 * @todo Merge rpmQueryFlags, rpmVerifyFlags, and rpmVerifyAttrs values?.
 */
typedef enum rpmVerifyFlags_e {
/*@-enummemuse@*/
    VERIFY_DEFAULT	= 0,		/*!< */
/*@=enummemuse@*/
    VERIFY_MD5		= (1 << 0),	/*!< from --nomd5 */
    VERIFY_SIZE		= (1 << 1),	/*!< from --nosize */
    VERIFY_LINKTO	= (1 << 2),	/*!< from --nolinkto */
    VERIFY_USER		= (1 << 3),	/*!< from --nouser */
    VERIFY_GROUP	= (1 << 4),	/*!< from --nogroup */
    VERIFY_MTIME	= (1 << 5),	/*!< from --nomtime */
    VERIFY_MODE		= (1 << 6),	/*!< from --nomode */
    VERIFY_RDEV		= (1 << 7),	/*!< from --nodev */
	/* bits 8-15 unused, reserved for rpmVerifyAttrs */
    VERIFY_FILES	= (1 << 16),	/*!< verify: from --nofiles */
    VERIFY_DEPS		= (1 << 17),	/*!< verify: from --nodeps */
    VERIFY_SCRIPT	= (1 << 18),	/*!< verify: from --noscripts */
    VERIFY_DIGEST	= (1 << 19),	/*!< verify: from --nodigest */
	/* bits 20-22 unused */
/*@-enummemuse@*/
    VERIFY_FOR_LIST	= (1 << 23),	/*!< query:  from --list */
    VERIFY_FOR_STATE	= (1 << 24),	/*!< query:  from --state */
    VERIFY_FOR_DOCS	= (1 << 25),	/*!< query:  from --docfiles */
    VERIFY_FOR_CONFIG	= (1 << 26),	/*!< query:  from --configfiles */
    VERIFY_FOR_DUMPFILES= (1 << 27),	/*!< query:  from --dump */
/*@=enummemuse@*/
	/* bits 28-30 used in rpmVerifyAttrs */
} rpmVerifyFlags;

#define	VERIFY_ATTRS	\
  ( VERIFY_MD5 | VERIFY_SIZE | VERIFY_LINKTO | VERIFY_USER | VERIFY_GROUP | \
    VERIFY_MTIME | VERIFY_MODE | VERIFY_RDEV )
#define	VERIFY_ALL	\
  ( VERIFY_ATTRS | VERIFY_FILES | VERIFY_DEPS | VERIFY_SCRIPT | VERIFY_DIGEST )

/** \ingroup rpmcli
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for query/verify
 */
typedef	int (*QVF_t) (QVA_t qva, rpmdb db, Header h)
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmcli
 * Display query/verify information for each header in iterator.
 * @param qva		parsed query/verify options
 * @param mi		rpm database iterator
 * @param showPackage	query/verify display routine
 * @return		result of last non-zero showPackage() return
 */
int showMatches(QVA_t qva, /*@only@*/ /*@null@*/ rpmdbMatchIterator mi,
		QVF_t showPackage)
	/*@modifies mi @*/;

/** \ingroup rpmcli
 * Display list of tags that can be used in --queryformat.
 * @param fp	file handle to use for display
 */
void rpmDisplayQueryTags(FILE * fp)
	/*@modifies *fp, fileSystem @*/;

/** \ingroup rpmcli
 * Common query/verify source interface, called once for each CLI arg.
 * @param qva		parsed query/verify options
 * @param source	type of source to query/verify
 * @param arg		name of source to query/verify
 * @param db		rpm database
 * @param showPackage	query/verify specific display routine
 * @return		showPackage() result, 1 if rpmdbInitIterator() is NULL
 */
int rpmQueryVerify(QVA_t qva, rpmQVSources source, const char * arg,
		rpmdb db, QVF_t showPackage)
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmcli
 * Display results of package query.
 * @todo Devise a meaningful return code.
 * @param qva		parsed query/verify options
 * @param db		rpm database (unused for queries)
 * @param h		header to use for query
 * @return		0 always
 */
int showQueryPackage(QVA_t qva, rpmdb db, Header h)
	/*@modifies db, fileSystem @*/;

/** \ingroup rpmcli
 * Display package information.
 * @todo hack: RPMQV_ALL can pass char ** arglist = NULL, not char * arg. Union?
 * @param qva		parsed query/verify options
 * @param source	type of source to query
 * @param arg		name of source to query
 * @return		rpmQueryVerify() result, or 1 on rpmdbOpen() failure
 */
int rpmQuery(QVA_t qva, rpmQVSources source, const char * arg)
	/*@modifies fileSystem @*/;

/** \ingroup rpmcli
 * Display results of package verify.
 * @param qva		parsed query/verify options
 * @param db		rpm database
 * @param h		header to use for verify
 * @return		result of last non-zero verify return
 */
int showVerifyPackage(QVA_t qva, /*@only@*/ rpmdb db, Header h)
	/*@modifies db, h, fileSystem @*/;

/** \ingroup rpmcli
 * Verify package install.
 * @param qva		parsed query/verify options
 * @param source	type of source to verify
 * @param arg		name of source to verify
 * @return		rpmQueryVerify() result, or 1 on rpmdbOpen() failure
 */
int rpmVerify(QVA_t qva, rpmQVSources source, const char *arg)
	/*@modifies fileSystem @*/;

/** \ingroup rpmcli
 * Describe query/verify command line request.
 */
struct rpmQVArguments_s {
    rpmQVSources qva_source;	/*!< Identify CLI arg type. */
    int 	qva_sourceCount;/*!< Exclusive check (>1 is error). */
    rpmQueryFlags qva_flags;	/*!< Bit(s) to control operation. */
    rpmfileAttrs qva_fflags;	/*!< Bit(s) to filter on attribute. */
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
 * Install/upgrade/freshen binary rpm package.
 * @param rootdir	path to top of install tree
 * @param fileArgv	array of package file names (NULL terminated)
 * @param transFlags	bits to control rpmRunTransactions()
 * @param interfaceFlags bits to control rpmInstall()
 * @param probFilter 	bits to filter problem types
 * @param relocations	package file relocations
 * @return		0 on success
 */
int rpmInstall(/*@null@*/ const char * rootdir,
		/*@null@*/ const char ** fileArgv,
		rpmtransFlags transFlags, 
		rpmInstallInterfaceFlags interfaceFlags,
		rpmprobFilterFlags probFilter,
		/*@null@*/ rpmRelocation * relocations)
	/*@modifies *relocations, fileSystem @*/;

/** \ingroup rpmcli
 * Install source rpm package.
 * @param rootdir	path to top of install tree
 * @param arg		source rpm file name
 * @retval specFile	address of (installed) spec file name
 * @retval cookie
 * @return		0 on success
 */
int rpmInstallSource(const char * rootdir, const char * arg,
		/*@null@*/ /*@out@*/ const char ** specFile,
		/*@null@*/ /*@out@*/ char ** cookie)
	/*@modifies *specFile, *cookie, fileSystem @*/;

/** \ingroup rpmcli
 * Erase binary rpm package.
 * @param rootdir	path to top of install tree
 * @param argv		array of package file names (NULL terminated)
 * @param transFlags	bits to control rpmRunTransactions()
 * @param interfaceFlags bits to control rpmInstall()
 * @return		0 on success
 */
int rpmErase(/*@null@*/ const char * rootdir, /*@null@*/ const char ** argv,
		rpmtransFlags transFlags, 
		rpmEraseInterfaceFlags interfaceFlags)
	/*@modifies fileSystem @*/;

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
 * Bit(s) to control rpmCheckSig() operation.
 */
typedef enum rpmCheckSigFlags_e {
/*@-enummemuse@*/
    CHECKSIG_NONE	= 0,	/*!< Don't check any signatures. */
/*@=enummemuse@*/
    CHECKSIG_PGP	= (1 << 0),	/*!< if not --nopgp */
    CHECKSIG_MD5	= (1 << 1),	/*!< if not --nomd5 */
    CHECKSIG_GPG	= (1 << 2)	/*!< if not --nogpg */
} rpmCheckSigFlags;
#define	CHECKSIG_ALL	(CHECKSIG_PGP|CHECKSIG_MD5|CHECKSIG_GPG)

/** \ingroup rpmcli
 * Check elements in signature header.
 * @param flags		bit(s) to enable signature checks
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
int rpmCheckSig(rpmCheckSigFlags flags, /*@null@*/ const char ** argv)
	/*@modifies fileSystem @*/;

/** \ingroup rpmcli
 * Bit(s) to control rpmReSign() operation.
 */
typedef enum rpmResignFlags_e {
    RESIGN_CHK_SIGNATURE = 0,	/*!< from --checksig */
    RESIGN_NEW_SIGNATURE,	/*!< from --resign */
    RESIGN_ADD_SIGNATURE	/*!< from --addsign */
} rpmResignFlags;

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param add		type of signature operation
 * @param passPhrase
 * @param argv		array of package file names (NULL terminated)
 * @return		0 on success
 */
int rpmReSign(rpmResignFlags add, char * passPhrase,
		/*@null@*/ const char ** argv)
	/*@modifies fileSystem @*/;

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
