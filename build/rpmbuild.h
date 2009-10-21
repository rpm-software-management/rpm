#ifndef	_H_RPMBUILD_
#define	_H_RPMBUILD_

/** \ingroup rpmbuild
 * \file build/rpmbuild.h
 *  This is the *only* module users of librpmbuild should need to include.
 */

#include <rpm/rpmcli.h>
#include <rpm/rpmds.h>

/* and it shouldn't need these :-( */
#include <rpm/rpmstring.h>

/* but this will be needed */
#include <rpm/rpmspec.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 * Bit(s) to control buildSpec() operation.
 */
typedef enum rpmBuildFlags_e {
    RPMBUILD_NONE	= 0,
    RPMBUILD_PREP	= (1 <<  0),	/*!< Execute %%prep. */
    RPMBUILD_BUILD	= (1 <<  1),	/*!< Execute %%build. */
    RPMBUILD_INSTALL	= (1 <<  2),	/*!< Execute %%install. */
    RPMBUILD_CHECK	= (1 <<  3),	/*!< Execute %%check. */
    RPMBUILD_CLEAN	= (1 <<  4),	/*!< Execute %%clean. */
    RPMBUILD_FILECHECK	= (1 <<  5),	/*!< Check %%files manifest. */
    RPMBUILD_PACKAGESOURCE = (1 <<  6),	/*!< Create source package. */
    RPMBUILD_PACKAGEBINARY = (1 <<  7),	/*!< Create binary package(s). */
    RPMBUILD_RMSOURCE	= (1 <<  8),	/*!< Remove source(s) and patch(s). */
    RPMBUILD_RMBUILD	= (1 <<  9),	/*!< Remove build sub-tree. */
    RPMBUILD_STRINGBUF	= (1 << 10),	/*!< only for doScript() */
    RPMBUILD_RMSPEC	= (1 << 11)	/*!< Remove spec file. */
} rpmBuildFlags;

#define PART_SUBNAME  0
#define PART_NAME     1

/** \ingroup rpmbuild
 * rpmSpec file parser states.
 */
/** \ingroup rpmbuild 
 *  * Spec file parser states. 
 *   */ 
#define PART_BASE       0 
typedef enum rpmParseState_e { 
    PART_ERROR          =  -1, /*!< */ 
    PART_NONE           =  0+PART_BASE, /*!< */ 
    /* leave room for RPMRC_NOTFOUND returns. */ 
    PART_PREAMBLE       = 11+PART_BASE, /*!< */ 
    PART_PREP           = 12+PART_BASE, /*!< */ 
    PART_BUILD          = 13+PART_BASE, /*!< */ 
    PART_INSTALL        = 14+PART_BASE, /*!< */ 
    PART_CHECK          = 15+PART_BASE, /*!< */ 
    PART_CLEAN          = 16+PART_BASE, /*!< */ 
    PART_FILES          = 17+PART_BASE, /*!< */ 
    PART_PRE            = 18+PART_BASE, /*!< */ 
    PART_POST           = 19+PART_BASE, /*!< */ 
    PART_PREUN          = 20+PART_BASE, /*!< */ 
    PART_POSTUN         = 21+PART_BASE, /*!< */ 
    PART_PRETRANS       = 22+PART_BASE, /*!< */ 
    PART_POSTTRANS      = 23+PART_BASE, /*!< */ 
    PART_DESCRIPTION    = 24+PART_BASE, /*!< */ 
    PART_CHANGELOG      = 25+PART_BASE, /*!< */ 
    PART_TRIGGERIN      = 26+PART_BASE, /*!< */ 
    PART_TRIGGERUN      = 27+PART_BASE, /*!< */ 
    PART_VERIFYSCRIPT   = 28+PART_BASE, /*!< */ 
    PART_BUILDARCHITECTURES= 29+PART_BASE,/*!< */ 
    PART_TRIGGERPOSTUN  = 30+PART_BASE, /*!< */ 
    PART_TRIGGERPREIN   = 31+PART_BASE, /*!< */ 
    PART_LAST           = 32+PART_BASE  /*!< */ 
} rpmParseState; 


#define STRIP_NOTHING             0
#define STRIP_TRAILINGSPACE (1 << 0)
#define STRIP_COMMENTS      (1 << 1)

/** \ingroup rpmbuild
 * Destroy uid/gid caches.
 */
void freeNames(void);

/** \ingroup rpmbuild
 * Return cached user name from user id.
 * @todo Implement using hash.
 * @param uid		user id
 * @return		cached user name
 */
const char * getUname(uid_t uid);

/** \ingroup rpmbuild
 * Return cached user name.
 * @todo Implement using hash.
 * @param uname		user name
 * @return		cached user name
 */
const char * getUnameS(const char * uname);

/** \ingroup rpmbuild
 * Return cached user id.
 * @todo Implement using hash.
 * @param uname		user name
 * @return		cached uid
 */
uid_t getUidS(const char * uname);

/** \ingroup rpmbuild
 * Return cached group name from group id.
 * @todo Implement using hash.
 * @param gid		group id
 * @return		cached group name
 */
const char * getGname(gid_t gid);

/** \ingroup rpmbuild
 * Return cached group name.
 * @todo Implement using hash.
 * @param gname		group name
 * @return		cached group name
 */
const char * getGnameS(const char * gname);

/** \ingroup rpmbuild
 * Return cached group id.
 * @todo Implement using hash.
 * @param gname		group name
 * @return		cached gid
 */
gid_t getGidS(const char * gname);

/** \ingroup rpmbuild
 * Return build hostname.
 * @return		build hostname
 */
const char * buildHost(void)	;

/** \ingroup rpmbuild
 * Return build time stamp.
 * @return		build time stamp
 */
rpm_time_t * getBuildTime(void)	;

/** \ingroup rpmbuild
 * Read next line from spec file.
 * @param spec		spec file control structure
 * @param strip		truncate comments?
 * @return		0 on success, 1 on EOF, <0 on error
 */
int readLine(rpmSpec spec, int strip);

/** \ingroup rpmbuild
 * Stop reading from spec file, freeing resources.
 * @param spec		spec file control structure
 */
void closeSpec(rpmSpec spec);

/** \ingroup rpmbuild
 * Truncate comment lines.
 * @param s		skip white space, truncate line at '#'
 */
void handleComments(char * s);

/** \ingroup rpmbuild
 * Check line for section separator, return next parser state.
 * @param		line from spec file
 * @return		next parser state
 */
rpmParseState isPart(const char * line)	;

/** \ingroup rpmbuild
 * Parse an unsigned number.
 * @param		line from spec file
 * @retval res		pointer to uint32_t
 * @return		0 on success, 1 on failure
 */
uint32_t parseUnsignedNum(const char * line, uint32_t * res);

/** \ingroup rpmbuild
 * Add changelog entry to header.
 * @param h		header
 * @param time		time of change
 * @param name		person who made the change
 * @param text		description of change
 */
void addChangelogEntry(Header h, time_t time, const char * name,
		const char * text);

/** \ingroup rpmbuild
 * Parse %%build/%%install/%%clean section(s) of a spec file.
 * @param spec		spec file control structure
 * @param parsePart	current rpmParseState
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseBuildInstallClean(rpmSpec spec, rpmParseState parsePart);

/** \ingroup rpmbuild
 * Parse %%changelog section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseChangelog(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse %%description section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseDescription(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse %%files section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseFiles(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse tags from preamble of a spec file.
 * @param spec		spec file control structure
 * @param initialPackage
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parsePreamble(rpmSpec spec, int initialPackage);

/** \ingroup rpmbuild
 * Parse %%prep section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parsePrep(rpmSpec spec);

/** \ingroup rpmbuild
 * Check for inappropriate characters. All alphanums are considered sane.
 * @param spec          spec
 * @param field         string to check
 * @param fsize         size of string to check
 * @param whitelist     string of permitted characters
 * @return              RPMRC_OK if OK
 */
rpmRC rpmCharCheck(rpmSpec spec, char *field, size_t fsize, const char *whitelist);

/** \ingroup rpmbuild
 * Parse dependency relations from spec file and/or autogenerated output buffer.
 * @param spec		spec file control structure
 * @param pkg		package control structure
 * @param field		text to parse (e.g. "foo < 0:1.2-3, bar = 5:6.7")
 * @param tagN		tag, identifies type of dependency
 * @param index		(0 always)
 * @param tagflags	dependency flags already known from context
 * @return		RPMRC_OK on success, RPMRC_FAIL on failure
 */
rpmRC parseRCPOT(rpmSpec spec, Package pkg, const char * field, rpmTag tagN,
		int index, rpmsenseFlags tagflags);

/** \ingroup rpmbuild
 * Parse %%pre et al scriptlets from a spec file.
 * @param spec		spec file control structure
 * @param parsePart	current rpmParseState
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseScript(rpmSpec spec, int parsePart);

/** \ingroup rpmbuild
 * Evaluate boolean expression.
 * @param spec		spec file control structure
 * @param expr		expression to parse
 * @return
 */
int parseExpressionBoolean(rpmSpec spec, const char * expr);

/** \ingroup rpmbuild
 * Evaluate string expression.
 * @param spec		spec file control structure
 * @param expr		expression to parse
 * @return
 */
char * parseExpressionString(rpmSpec spec, const char * expr);

/** \ingroup rpmbuild
 * Remove all sources assigned to spec file.
 *
 * @param spec		spec file control structure
 * @return		RPMRC_OK on success
 */
rpmRC doRmSource(rpmSpec spec);
/** \ingroup rpmbuild
 * Run a build script, assembled from spec file scriptlet section.
 *
 * @param spec		spec file control structure
 * @param what		type of script
 * @param name		name of scriptlet section
 * @param sb		lines that compose script body
 * @param test		don't execute scripts or package if testing
 * @return		RPMRC_OK on success
 */
rpmRC doScript(rpmSpec spec, rpmBuildFlags what, const char * name,
		StringBuf sb, int test);

/** \ingroup rpmbuild
 * Find sub-package control structure by name.
 * @param spec		spec file control structure
 * @param name		(sub-)package name
 * @param flag		if PART_SUBNAME, then 1st package name is prepended
 * @retval pkg		package control structure
 * @return		0 on success, 1 on failure
 */
rpmRC lookupPackage(rpmSpec spec, const char * name, int flag,
		Package * pkg);

/** \ingroup rpmbuild
 * Create and initialize package control structure.
 * @param spec		spec file control structure
 * @return		package control structure
 */
Package newPackage(rpmSpec spec);

/** \ingroup rpmbuild
 * Destroy all packages associated with spec file.
 * @param packages	package control structure chain
 * @return		NULL
 */
Package freePackages(Package packages);

/** \ingroup rpmbuild
 * Destroy package control structure.
 * @param pkg		package control structure
 * @return		NULL
 */
Package  freePackage(Package pkg);

/** \ingroup rpmbuild
 * Add dependency to header, filtering duplicates.
 * @param spec		spec file control structure
 * @param h		header
 * @param tagN		tag, identifies type of dependency
 * @param N		(e.g. Requires: foo < 0:1.2-3, "foo")
 * @param EVR		(e.g. Requires: foo < 0:1.2-3, "0:1.2-3")
 * @param Flags		(e.g. Requires: foo < 0:1.2-3, both "Requires:" and "<")
 * @param index		(0 always)
 * @return		0 on success, 1 on error
 */
int addReqProv(rpmSpec spec, Header h, rpmTag tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		uint32_t index);

/** \ingroup rpmbuild
 * Add rpmlib feature dependency.
 * @param h		header
 * @param feature	rpm feature name (i.e. "rpmlib(Foo)" for feature Foo)
 * @param featureEVR	rpm feature epoch/version/release
 * @return		0 always
 */
int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR);

/** \ingroup rpmbuild
 * Post-build processing for binary package(s).
 * @param spec		spec file control structure
 * @param installSpecialDoc
 * @param test		don't execute scripts or package if testing
 * @return		0 on success
 */
int processBinaryFiles(rpmSpec spec, int installSpecialDoc, int test);

/** \ingroup rpmbuild
 * Create and initialize header for source package.
 * @param spec		spec file control structure
 */
void initSourceHeader(rpmSpec spec);

/** \ingroup rpmbuild
 * Post-build processing for source package.
 * @param spec		spec file control structure
 * @return		0 on success
 */
int processSourceFiles(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse spec file into spec control structure.
 * @param ts		transaction set (spec file control in ts->spec)
 * @param specFile
 * @param rootDir
 * @param buildRoot
 * @param recursing	parse is recursive?
 * @param passPhrase
 * @param cookie
 * @param anyarch
 * @param force
 * @return
 */
int parseSpec(rpmts ts, const char * specFile,
		const char * rootDir,
		const char * buildRoot,
		int recursing,
		const char * passPhrase,
		const char * cookie,
		int anyarch, int force);

/** \ingroup rpmbuild
 * Build stages state machine driver.
 * @param ts		transaction set
 * @param spec		spec file control structure
 * @param what		bit(s) to enable stages of build
 * @param test		don't execute scripts or package if testing
 * @return		RPMRC_OK on success
 */
rpmRC buildSpec(rpmts ts, rpmSpec spec, int what, int test);

/** \ingroup rpmbuild
 * Check package(s).
 * @param pkgcheck	program to run
 * @return		RPMRC_OK on success
 */
rpmRC checkPackages(char *pkgcheck);

/** \ingroup rpmbuild
 * Generate binary package(s).
 * @param spec		spec file control structure
 * @return		RPMRC_OK on success
 */
rpmRC packageBinaries(rpmSpec spec);

/** \ingroup rpmbuild
 * Generate source package.
 * @param spec		spec file control structure
 * @return		RPMRC_OK on success
 */
rpmRC packageSources(rpmSpec spec);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMBUILD_ */
