#ifndef	_H_RPMBUILD_
#define	_H_RPMBUILD_

/** \ingroup rpmbuild
 * \file build/rpmbuild.h
 *  This is the *only* module users of librpmbuild should need to include.
 */

#include "rpmlib.h"

/* and it shouldn't need these :-( */
#include "stringbuf.h"
#include "misc.h"

/* but this will be needed */
#include "rpmspec.h"

/**
 * Bit(s) to control buildSpec() operation.
 */
typedef enum rpmBuildFlags_e {
    RPMBUILD_PREP	= (1 << 0),	/*!< Execute %%prep. */
    RPMBUILD_BUILD	= (1 << 1),	/*!< Execute %%build. */
    RPMBUILD_INSTALL	= (1 << 2),	/*!< Execute %%install. */
    RPMBUILD_CLEAN	= (1 << 3),	/*!< Execute %%clean. */
    RPMBUILD_FILECHECK	= (1 << 4),	/*!< Check %%files manifest. */
    RPMBUILD_PACKAGESOURCE = (1 << 5),	/*!< Create source package. */
    RPMBUILD_PACKAGEBINARY = (1 << 6),	/*!< Create binary package(s). */
    RPMBUILD_RMSOURCE	= (1 << 7),	/*!< Remove source(s) and patch(s). */
    RPMBUILD_RMBUILD	= (1 << 8),	/*!< Remove build sub-tree. */
    RPMBUILD_STRINGBUF	= (1 << 9),	/*!< only for doScript() */
    RPMBUILD_RMSPEC	= (1 << 10)	/*!< Remove spec file. */
} rpmBuildFlags;

/* from build/misc.h */

#include <ctype.h>

#define FREE(x) { if (x) free((void *)x); x = NULL; }
#define SKIPSPACE(s) { while (*(s) && isspace(*(s))) (s)++; }
#define SKIPNONSPACE(s) { while (*(s) && !isspace(*(s))) (s)++; }

#define PART_SUBNAME  0
#define PART_NAME     1

/**
 * Spec file parser states.
 */
typedef enum rpmParseState_e {
    PART_NONE		= 0,	/*!< */
    PART_PREAMBLE	= 1,	/*!< */
    PART_PREP		= 2,	/*!< */
    PART_BUILD		= 3,	/*!< */
    PART_INSTALL	= 4,	/*!< */
    PART_CLEAN		= 5,	/*!< */
    PART_FILES		= 6,	/*!< */
    PART_PRE		= 7,	/*!< */
    PART_POST		= 8,	/*!< */
    PART_PREUN		= 9,	/*!< */
    PART_POSTUN		= 10,	/*!< */
    PART_DESCRIPTION	= 11,	/*!< */
    PART_CHANGELOG	= 12,	/*!< */
    PART_TRIGGERIN	= 13,	/*!< */
    PART_TRIGGERUN	= 14,	/*!< */
    PART_VERIFYSCRIPT	= 15,	/*!< */
    PART_BUILDARCHITECTURES= 16,/*!< */
    PART_TRIGGERPOSTUN	= 17	/*!< */
} rpmParseState;

/* from build/read.h */

#define STRIP_NOTHING             0
#define STRIP_TRAILINGSPACE (1 << 0)
#define STRIP_COMMENTS      (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* from build/names.h */

/** */
void freeNames(void);

/** */
/*@observer@*/ const char *getUname(uid_t uid);

/** */
/*@observer@*/ const char *getUnameS(const char *uname);

/** */
/*@observer@*/ const char *getGname(gid_t gid);

/** */
/*@observer@*/ const char *getGnameS(const char *gname);

/** */
/*@observer@*/ const char *const buildHost(void);

/** */
/*@observer@*/ time_t *const getBuildTime(void);

/* from build/read.h */

/**
 * @return		0 on success, 1 on EOF, <0 on error
 */
int readLine(Spec spec, int strip);

/** */
void closeSpec(Spec spec);

/** */
void handleComments(char *s);

/* from build/part.h */

/**
 * Check line for section separator, return next parser state.
 * @param		line from spec file
 * @return		next parser state
 */
rpmParseState isPart(const char *line);

/* from build/misc.h */

/** */
int parseNum(const char *line, /*@out@*/int *res);

/* from build/parse.h */

/** */
void addChangelogEntry(Header h, time_t time, const char *name, const char *text);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseBuildInstallClean(Spec spec, rpmParseState parsePart);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseChangelog(Spec spec);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseDescription(Spec spec);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseFiles(Spec spec);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parsePreamble(Spec spec, int initialPackage);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parsePrep(Spec spec);

/** */
int parseRCPOT(Spec spec, Package pkg, const char *field, int tag, int index,
	       int flags);

/**
 * @return		>= 0 next rpmParseState, < 0 on error
 */
int parseScript(Spec spec, int parsePart);

/** */
int parseTrigger(Spec spec, Package pkg, char *field, int tag);

/* from build/expression.h */

/** */
int parseExpressionBoolean(Spec, char *);

/** */
char *parseExpressionString(Spec, char *);

/* from build/build.h */

/** */
int doScript(Spec spec, int what, const char *name, StringBuf sb, int test);

/* from build/package.h */

/** */
int lookupPackage(Spec spec, const char *name, int flag, /*@out@*/Package *pkg);

/** */
/*@only@*/ Package newPackage(Spec spec);

/** */
void freePackages(Spec spec);

/** */
void freePackage(/*@only@*/ Package p);

/* from build/reqprov.h */

/** */
int addReqProv(/*@unused@*/Spec spec, Header h,
		int flag, const char *name, const char *version, int index);

/** */
int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR);

/* from build/files.h */

/** */
int processBinaryFiles(Spec spec, int installSpecialDoc, int test);

/** */
void initSourceHeader(Spec spec);

/** */
int processSourceFiles(Spec spec);

/* global entry points */

/** */
int parseSpec(Spec *specp, const char *specFile, const char *rootdir,
		const char *buildRoot, int inBuildArch, const char *passPhrase,
		char *cookie, int anyarch, int force);

/** */
extern int (*parseSpecVec) (Spec *specp, const char *specFile, const char *rootdir,
		const char *buildRoot, int inBuildArch, const char *passPhrase,
		char *cookie, int anyarch, int force);	/* XXX FIXME */

/** */
int buildSpec(Spec spec, int what, int test);

/** */
int packageBinaries(Spec spec);

/** */
int packageSources(Spec spec);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMBUILD_ */
