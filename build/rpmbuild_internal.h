#ifndef _RPMBUILD_INTERNAL_H
#define _RPMBUILD_INTERNAL_H

#include <rpm/rpmbuild.h>
#include <rpm/rpmutil.h>
#include <rpm/rpmstrpool.h>
#include "build/rpmbuild_misc.h"

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE fileRenameHash
#define HTKEYTYPE const char *
#define HTDATATYPE const char *
#include "lib/rpmhash.H"
#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE

#define WHITELIST_NAME ".-_+%{}"
#define WHITELIST_VERREL "._+%{}~^"
#define WHITELIST_EVR WHITELIST_VERREL "-:"

struct TriggerFileEntry {
    int index;
    char * fileName;
    char * script;
    char * prog;
    uint32_t flags;
    struct TriggerFileEntry * next;
    uint32_t priority;
};

typedef struct ReadLevelEntry {
    int reading;
    int lineNum;
    int elifEnabled;
    int readable;
    struct ReadLevelEntry * next;
} RLE_t;

/** \ingroup rpmbuild
 */
struct Source {
    char * fullSource;
    const char * source;     /* Pointer into fullSource */
    int flags;
    uint32_t num;
struct Source * next;
};

typedef struct Package_s * Package;

/** \ingroup rpmbuild
 * The structure used to store values parsed from a spec file.
 */
struct rpmSpec_s {
    char * buildHost;
    rpm_time_t buildTime;

    char * specFile;	/*!< Name of the spec file. */
    char * buildRoot;
    char * buildSubdir;
    const char * rootDir;

    struct OpenFileInfo * fileStack;
    char *lbuf;
    size_t lbufSize;
    size_t lbufOff;
    char nextpeekc;
    char * nextline;
    char * line;
    int lineNum;

    struct ReadLevelEntry * readStack;

    Header buildRestrictions;
    rpmSpec * BASpecs;
    const char ** BANames;
    int BACount;
    int recursing;		/*!< parse is recursive? */

    rpmSpecFlags flags;

    struct Source * sources;
    int numSources;
    int noSource;
    int autonum_patch;
    int autonum_source;

    char * sourceRpmName;
    unsigned char * sourcePkgId;
    Package sourcePackage;

    rpmMacroContext macros;
    rpmstrPool pool;

    StringBuf prep;		/*!< %prep scriptlet. */
    StringBuf build;		/*!< %build scriptlet. */
    StringBuf install;		/*!< %install scriptlet. */
    StringBuf check;		/*!< %check scriptlet. */
    StringBuf clean;		/*!< %clean scriptlet. */

    StringBuf parsed;		/*!< parsed spec contents */

    Package packages;		/*!< Package list. */
};

#define PACKAGE_NUM_DEPS 12

/** \ingroup rpmbuild
 * The structure used to store values for a package.
 */
struct Package_s {
    rpmsid name;
    rpmstrPool pool;
    Header header;
    rpmds ds;			/*!< Requires: N = EVR */
    rpmds dependencies[PACKAGE_NUM_DEPS];
    rpmfiles cpioList;
    ARGV_t dpaths;

    struct Source * icon;

    int autoReq;
    int autoProv;

    char * preInFile;	/*!< %pre scriptlet. */
    char * postInFile;	/*!< %post scriptlet. */
    char * preUnFile;	/*!< %preun scriptlet. */
    char * postUnFile;	/*!< %postun scriptlet. */
    char * preTransFile;	/*!< %pretrans scriptlet. */
    char * postTransFile;	/*!< %posttrans scriptlet. */
    char * verifyFile;	/*!< %verifyscript scriptlet. */

    struct TriggerFileEntry * triggerFiles;
    struct TriggerFileEntry * fileTriggerFiles;
    struct TriggerFileEntry * transFileTriggerFiles;

    ARGV_t fileFile;
    ARGV_t fileList;		/* If NULL, package will not be written */
    ARGV_t fileExcludeList;
    ARGV_t removePostfixes;
    fileRenameHash fileRenameMap;
    ARGV_t policyList;

    Package next;
};

#define PART_SUBNAME  0
#define PART_NAME     1
#define PART_QUIET    2

/** \ingroup rpmbuild
 * rpmSpec file parser states.
 */
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
    PART_POLICIES       = 32+PART_BASE, /*!< */
    PART_FILETRIGGERIN		= 33+PART_BASE, /*!< */
    PART_FILETRIGGERUN		= 34+PART_BASE, /*!< */
    PART_FILETRIGGERPOSTUN	= 35+PART_BASE, /*!< */
    PART_TRANSFILETRIGGERIN	= 36+PART_BASE, /*!< */
    PART_TRANSFILETRIGGERUN	= 37+PART_BASE, /*!< */
    PART_TRANSFILETRIGGERPOSTUN	= 38+PART_BASE, /*!< */
    PART_EMPTY			= 39+PART_BASE, /*!< */
    PART_LAST			= 40+PART_BASE  /*!< */
} rpmParseState; 


/** \ingroup rpmbuild
 * Bit(s) to control type of the current spec file line
 */
enum ifTypes {
    OTHER_LINE = 0,        /*!< other lines                                        */
    IF_LINE    = (1 << 0), /*!< %if or %ifos or %ifnos or %ifarch or %ifnarch line */
    ELIF_LINE  = (1 << 1)  /*!< %elif or %elifos or %elifarch line                 */
};

#define STRIP_NOTHING             0
#define STRIP_TRAILINGSPACE (1 << 0)
#define STRIP_COMMENTS      (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 * Create and initialize rpmSpec structure.
 * @return spec		spec file control structure
 */
RPM_GNUC_INTERNAL
rpmSpec newSpec(void);

/** \ingroup rpmbuild
 * Stop reading from spec file, freeing resources.
 * @param spec		spec file control structure
 */
RPM_GNUC_INTERNAL
void closeSpec(rpmSpec spec);

/** \ingroup rpmbuild
 * Read next line from spec file.
 * @param spec		spec file control structure
 * @param strip		truncate comments?
 * @return		0 on success, 1 on EOF, <0 on error
 */
RPM_GNUC_INTERNAL
int readLine(rpmSpec spec, int strip);

/** \ingroup rpmbuild
 * Check line for section separator, return next parser state.
 * @param		line from spec file
 * @return		next parser state
 */
RPM_GNUC_INTERNAL
int isPart(const char * line)	;

/** \ingroup rpmbuild
 * Parse %%build/%%install/%%clean section(s) of a spec file.
 * @param spec		spec file control structure
 * @param parsePart	current rpmParseState
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parseSimpleScript(rpmSpec spec, const char * name, StringBuf *sbp);

/** \ingroup rpmbuild
 * Parse %%changelog section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parseChangelog(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse %%description section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parseDescription(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse %%files section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parseFiles(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse %%sepolicy section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parsePolicies(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse tags from preamble of a spec file.
 * @param spec		spec file control structure
 * @param initialPackage
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parsePreamble(rpmSpec spec, int initialPackage);

/** \ingroup rpmbuild
 * Parse %%prep section of a spec file.
 * @param spec		spec file control structure
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parsePrep(rpmSpec spec);

/** \ingroup rpmbuild
 * Parse %%pre et al scriptlets from a spec file.
 * @param spec		spec file control structure
 * @param parsePart	current rpmParseState
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parseScript(rpmSpec spec, int parsePart);

/** \ingroup rpmbuild
 * Check for inappropriate characters. All alphanums are considered sane.
 * @param spec          spec
 * @param field         string to check
 * @param whitelist     string of permitted characters
 * @return              RPMRC_OK if OK
 */
RPM_GNUC_INTERNAL
rpmRC rpmCharCheck(rpmSpec spec, const char *field, const char *whitelist);

typedef rpmRC (*addReqProvFunction) (void *cbdata, rpmTagVal tagN,
				     const char * N, const char * EVR, rpmsenseFlags Flags,
				     int index);

/** \ingroup rpmbuild
 * Parse dependency relations from spec file and/or autogenerated output buffer.
 * @param spec		spec file control structure
 * @param pkg		package control structure
 * @param field		text to parse (e.g. "foo < 0:1.2-3, bar = 5:6.7")
 * @param tagN		tag, identifies type of dependency
 * @param index		(0 always)
 * @param tagflags	dependency flags already known from context
 * @param cb		Callback for adding dependency (nullable)
 * @param cbdata	Callback data (@pkg if NULL)
 * @return		RPMRC_OK on success, RPMRC_FAIL on failure
 */
RPM_GNUC_INTERNAL
rpmRC parseRCPOT(rpmSpec spec, Package pkg, const char * field, rpmTagVal tagN,
		int index, rpmsenseFlags tagflags, addReqProvFunction cb, void *cbdata);

/** \ingroup rpmbuild
 * Evaluate boolean expression.
 * @param expr		expression to parse
 * @return
 */
RPM_GNUC_INTERNAL
int parseExpressionBoolean(const char * expr);

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
RPM_GNUC_INTERNAL
rpmRC doScript(rpmSpec spec, rpmBuildFlags what, const char * name,
		const char * sb, int test);

/** \ingroup rpmbuild
 * Find sub-package control structure by name.
 * @param spec		spec file control structure
 * @param name		(sub-)package name
 * @param flag		if PART_SUBNAME, then 1st package name is prepended
 * @retval pkg		package control structure
 * @return		0 on success, 1 on failure
 */
RPM_GNUC_INTERNAL
rpmRC lookupPackage(rpmSpec spec, const char * name, int flag,
		Package * pkg);

/** \ingroup rpmbuild
 * Create and initialize package control structure.
 * @param name		package name for sub-packages (or NULL)
 * @param pool		string pool
 * @param pkglist	package list pointer to append to (or NULL)
 * @return		package control structure
 */
RPM_GNUC_INTERNAL
Package newPackage(const char *name, rpmstrPool pool, Package * pkglist);


/** \ingroup rpmbuild
 * Return rpmds containing the dependencies of a given type
 * @param pkg		package
 * @param tag		name tag denominating the dependency
 * @return		pointer to dependency set
 */
RPM_GNUC_INTERNAL
rpmds * packageDependencies(Package pkg, rpmTagVal tag);

/** \ingroup rpmbuild
 * Post-build processing for binary package(s).
 * @param spec		spec file control structure
 * @param pkgFlags	bit(s) to control package generation
 * @param didInstall	was %install executed?
 * @param test		don't execute scripts or package if testing
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC processBinaryFiles(rpmSpec spec, rpmBuildPkgFlags pkgFlags,
			int didInstall, int test);

/** \ingroup rpmfc
 * Generate package dependencies.
 * @param spec		spec file control
 * @param pkg		package control
 * @return		RPMRC_OK on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmfcGenerateDepends(const rpmSpec spec, Package pkg);

/** \ingroup rpmfc
 * Return helper output.
 * @param av		helper argv (with possible macros)
 * @param sb_stdin	helper input
 * @retval *sb_stdoutp	helper output
 * @param failnonzero	IS non-zero helper exit status a failure?
 * @param buildRoot	buildRoot directory (or NULL)
 */
RPM_GNUC_INTERNAL
int rpmfcExec(ARGV_const_t av, StringBuf sb_stdin, StringBuf * sb_stdoutp,
		int failnonzero, const char *buildRoot);

/** \ingroup rpmbuild
 * Post-build processing for policies in binary package(s).
 * @param spec		spec file control structure
 * @param test		don't execute scripts or package if testing
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC processBinaryPolicies(rpmSpec spec, int test);

/** \ingroup rpmbuild
 * Post-build processing for source package.
 * @param spec		spec file control structure
 * @param pkgFlags	bit(s) to control package generation
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC processSourceFiles(rpmSpec spec, rpmBuildPkgFlags pkgFlags);

/** \ingroup rpmbuild
 * Generate binary package(s).
 * @param spec		spec file control structure
 * @param cookie	build identifier "cookie" or NULL
 * @param cheating	was build shortcircuited?
 * @return		RPMRC_OK on success
 */
RPM_GNUC_INTERNAL
rpmRC packageBinaries(rpmSpec spec, const char *cookie, int cheating);

/** \ingroup rpmbuild
 * Generate source package.
 * @param spec		spec file control structure
 * @retval cookie	build identifier "cookie" or NULL
 * @return		RPMRC_OK on success
 */
RPM_GNUC_INTERNAL
rpmRC packageSources(rpmSpec spec, char **cookie);

RPM_GNUC_INTERNAL
int addLangTag(rpmSpec spec, Header h, rpmTagVal tag,
		const char *field, const char *lang);

/** \ingroup rpmbuild
 * Add dependency to package, filtering duplicates.
 * @param pkg		package
 * @param tagN		tag, identifies type of dependency
 * @param N		(e.g. Requires: foo < 0:1.2-3, "foo")
 * @param EVR		(e.g. Requires: foo < 0:1.2-3, "0:1.2-3")
 * @param Flags		(e.g. Requires: foo < 0:1.2-3, both "Requires:" and "<")
 * @param index         (# trigger script for triggers, 0 for others)
 * @return		0 on success, 1 on error
 */
RPM_GNUC_INTERNAL
int addReqProv(Package pkg, rpmTagVal tagN,
	       const char * N, const char * EVR, rpmsenseFlags Flags,
	       uint32_t index);

RPM_GNUC_INTERNAL
rpmRC addReqProvPkg(void *cbdata, rpmTagVal tagN,
		    const char * N, const char * EVR, rpmsenseFlags Flags,
		    int index);

/** \ingroup rpmbuild
 * Add self-provides to package.
 * @param pkg		package
 */
RPM_GNUC_INTERNAL
void addPackageProvides(Package pkg);

/** \ingroup rpmbuild
 * Add rpmlib feature dependency.
 * @param pkg		package
 * @param feature	rpm feature name (i.e. "rpmlib(Foo)" for feature Foo)
 * @param featureEVR	rpm feature epoch/version/release
 * @return		0 always
 */
RPM_GNUC_INTERNAL
int rpmlibNeedsFeature(Package pkg, const char * feature, const char * featureEVR);

RPM_GNUC_INTERNAL
rpmRC checkForEncoding(Header h, int addtag);


/** \ingroup rpmbuild
 * Copy tags inherited by subpackages from the source header to the target header
 * @param h		target header
 * @param fromh		source header
 */
RPM_GNUC_INTERNAL
void copyInheritedTags(Header h, Header fromh);

#ifdef __cplusplus
}
#endif

#endif /* _RPMBUILD_INTERNAL_H */
