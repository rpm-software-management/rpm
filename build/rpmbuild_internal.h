#ifndef _RPMBUILD_INTERNAL_H
#define _RPMBUILD_INTERNAL_H

#include <rpm/rpmbuild.h>
#include <rpm/rpmutil.h>

struct TriggerFileEntry {
    int index;
    char * fileName;
    char * script;
    char * prog;
    uint32_t flags;
    struct TriggerFileEntry * next;
};

typedef struct ReadLevelEntry {
    int reading;
    struct ReadLevelEntry * next;
} RLE_t;

typedef struct spectag_s {
    int t_tag;
    int t_startx;
    int t_nlines;
    char * t_lang;
    char * t_msgid;
} * spectag;

struct spectags_s {
    spectag st_t;
    int st_nalloc;
    int st_ntags;
};

struct speclines_s {
    char **sl_lines;
    int sl_nalloc;
    int sl_nlines;
};

#define PART_SUBNAME  0
#define PART_NAME     1

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
    PART_LAST           = 33+PART_BASE  /*!< */
} rpmParseState; 


#define STRIP_NOTHING             0
#define STRIP_TRAILINGSPACE (1 << 0)
#define STRIP_COMMENTS      (1 << 1)

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
rpmParseState isPart(const char * line)	;

/** \ingroup rpmbuild
 * Parse %%build/%%install/%%clean section(s) of a spec file.
 * @param spec		spec file control structure
 * @param parsePart	current rpmParseState
 * @return		>= 0 next rpmParseState, < 0 on error
 */
RPM_GNUC_INTERNAL
int parseBuildInstallClean(rpmSpec spec, rpmParseState parsePart);

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
 * @param fsize         size of string to check
 * @param whitelist     string of permitted characters
 * @return              RPMRC_OK if OK
 */
RPM_GNUC_INTERNAL
rpmRC rpmCharCheck(rpmSpec spec, char *field, size_t fsize, const char *whitelist);

/** \ingroup rpmbuild
 * stashSt.
 * @param spec		spec file control structure
 * @param h		header
 * @param tag		tag
 * @param lang		locale
 */
spectag stashSt(rpmSpec spec, Header h, rpmTag tag, const char * lang);

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
RPM_GNUC_INTERNAL
rpmRC parseRCPOT(rpmSpec spec, Package pkg, const char * field, rpmTag tagN,
		int index, rpmsenseFlags tagflags);

/** \ingroup rpmbuild
 * Evaluate boolean expression.
 * @param spec		spec file control structure
 * @param expr		expression to parse
 * @return
 */
RPM_GNUC_INTERNAL
int parseExpressionBoolean(rpmSpec spec, const char * expr);

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
		StringBuf sb, int test);

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
 * @param spec		spec file control structure
 * @return		package control structure
 */
RPM_GNUC_INTERNAL
Package newPackage(rpmSpec spec);

/** \ingroup rpmbuild
 * Post-build processing for binary package(s).
 * @param spec		spec file control structure
 * @param installSpecialDoc
 * @param test		don't execute scripts or package if testing
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int processBinaryFiles(rpmSpec spec, int installSpecialDoc, int test);

/** \ingroup rpmbuild
 * Post-build processing for policies in binary package(s).
 * @param spec		spec file control structure
 * @param test		don't execute scripts or package if testing
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int processBinaryPolicies(rpmSpec spec, int test);

/** \ingroup rpmbuild
 * Post-build processing for source package.
 * @param spec		spec file control structure
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
int processSourceFiles(rpmSpec spec);

/** \ingroup rpmbuild
 * Generate binary package(s).
 * @param spec		spec file control structure
 * @param cookie	build identifier "cookie" or NULL
 * @return		RPMRC_OK on success
 */
RPM_GNUC_INTERNAL
rpmRC packageBinaries(rpmSpec spec, const char *cookie);

/** \ingroup rpmbuild
 * Generate source package.
 * @param spec		spec file control structure
 * @retval cookie	build identifier "cookie" or NULL
 * @return		RPMRC_OK on success
 */
RPM_GNUC_INTERNAL
rpmRC packageSources(rpmSpec spec, char **cookie);

#endif /* _RPMBUILD_INTERNAL_H */
