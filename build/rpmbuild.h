#ifndef	_H_RPMBUILD_
#define	_H_RPMBUILD_

/* This is the *only* module users of librpmbuild should need to include */
#include "rpmlib.h"

/* and it shouldn't need these :-( */
#include "stringbuf.h"
#include "misc.h"

/* but this will be needed */
#include "rpmspec.h"

/* from build/build.h */

#define	RPMBUILD_PREP		(1 << 0)
#define	RPMBUILD_BUILD		(1 << 1)
#define	RPMBUILD_INSTALL	(1 << 2)
#define	RPMBUILD_CLEAN		(1 << 3)
#define	RPMBUILD_FILECHECK	(1 << 4)
#define	RPMBUILD_PACKAGESOURCE	(1 << 5)
#define	RPMBUILD_PACKAGEBINARY	(1 << 6)
#define	RPMBUILD_RMSOURCE	(1 << 7)
#define	RPMBUILD_RMBUILD	(1 << 8)
#define	RPMBUILD_STRINGBUF	(1 << 9) /* only for doScript() */
#define	RPMBUILD_RMSPEC		(1 << 10)

/* from build/misc.h */

#include <ctype.h>

#define FREE(x) { if (x) free((void *)x); x = NULL; }
#define SKIPSPACE(s) { while (*(s) && isspace(*(s))) (s)++; }
#define SKIPNONSPACE(s) { while (*(s) && !isspace(*(s))) (s)++; }

#define PART_SUBNAME  0
#define PART_NAME     1

/* from build/part.h */

#define PART_NONE                0
#define PART_PREAMBLE            1
#define PART_PREP                2
#define PART_BUILD               3
#define PART_INSTALL             4
#define PART_CLEAN               5
#define PART_FILES               6
#define PART_PRE                 7
#define PART_POST                8
#define PART_PREUN               9
#define PART_POSTUN             10
#define PART_DESCRIPTION        11
#define PART_CHANGELOG          12
#define PART_TRIGGERIN          13
#define PART_TRIGGERUN          14
#define PART_VERIFYSCRIPT       15
#define PART_BUILDARCHITECTURES 16
#define PART_TRIGGERPOSTUN      17

/* from build/read.h */

#define STRIP_NOTHING             0
#define STRIP_TRAILINGSPACE (1 << 0)
#define STRIP_COMMENTS      (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

/* from build/names.h */

/*@observer@*/ const char *getUname(uid_t uid);
/*@observer@*/ const char *getUnameS(const char *uname);
/*@observer@*/ const char *getGname(gid_t gid);
/*@observer@*/ const char *getGnameS(const char *gname);

/*@observer@*/ const char *const buildHost(void);
/*@observer@*/ time_t *const getBuildTime(void);

/* from build/read.h */

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */
int readLine(Spec spec, int strip);

void closeSpec(Spec spec);
void handleComments(char *s);

/* from build/part.h */

int isPart(char *line);

/* from build/misc.h */

int parseNum(const char *line, /*@out@*/int *res);
/*@observer@*/ const char *cleanFileName(const char *name);

/* from build/parse.h */

void addChangelogEntry(Header h, time_t time, const char *name, const char *text);
int parseChangelog(Spec spec);
int parseDescription(Spec spec);
int parseFiles(Spec spec);
int parsePreamble(Spec spec, int initialPackage);
int parsePrep(Spec spec);
int parseRCPOT(Spec spec, Package pkg, const char *field, int tag, int index);
int parseTrigger(Spec spec, Package pkg, char *field, int tag);
int parseScript(Spec spec, int parsePart);
int parseBuildInstallClean(Spec spec, int parsePart);

/* from build/expression.h */

int parseExpressionBoolean(Spec, char *);
char *parseExpressionString(Spec, char *);

/* from build/build.h */

int doScript(Spec spec, int what, const char *name, StringBuf sb, int test);

/* from build/package.h */

int lookupPackage(Spec spec, const char *name, int flag, /*@out@*/Package *pkg);
/*@only@*/ Package newPackage(Spec spec);
void freePackages(Spec spec);
void freePackage(/*@only@*/ Package p);

/* from build/reqprov.h */

int addReqProv(/*@unused@*/Spec spec, Header h,
		int flag, const char *name, const char *version, int index);

/* from build/files.h */

int processBinaryFiles(Spec spec, int installSpecialDoc, int test);
void initSourceHeader(Spec spec);
int processSourceFiles(Spec spec);

/* global entry points */

int parseSpec(Spec *specp, const char *specFile, const char *buildRoot,
		int inBuildArch, const char *passPhrase, char *cookie, int anyarch,
		int force);

extern int (*parseSpecVec) (Spec *specp, const char *specFile, const char *buildRoot,
		int inBuildArch, const char *passPhrase, char *cookie, int anyarch,
		int force);	/* XXX FIXME */

int buildSpec(Spec spec, int what, int test);

int packageBinaries(Spec spec);
int packageSources(Spec spec);

#ifdef __cplusplus
}
#endif

#endif	/* _H_RPMBUILD_ */
