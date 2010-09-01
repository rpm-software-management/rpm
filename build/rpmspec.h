#ifndef _H_SPEC_
#define _H_SPEC_

/** \ingroup rpmbuild
 * \file build/rpmspec.h
 *  The rpmSpec and Package data structures used during build.
 */

#include <rpm/rpmstring.h>	/* StringBuf */
#include <rpm/rpmcli.h>	/* for QVA_t */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 */
typedef struct Package_s * Package;

#define RPMBUILD_ISSOURCE	(1 << 0)
#define RPMBUILD_ISPATCH	(1 << 1)
#define RPMBUILD_ISICON		(1 << 2)
#define RPMBUILD_ISNO		(1 << 3)

#define RPMBUILD_DEFAULT_LANG "C"

/** \ingroup rpmbuild
 */
struct Source {
    char * fullSource;
    char * source;     /* Pointer into fullSource */
    int flags;
    uint32_t num;
struct Source * next;
};

typedef struct spectags_s * spectags;
typedef struct speclines_s * speclines;

/** \ingroup rpmbuild
 * The structure used to store values parsed from a spec file.
 */
struct rpmSpec_s {
    char * specFile;	/*!< Name of the spec file. */
    char * buildRoot;
    char * buildSubdir;
    char * rootDir;

    speclines sl;
    spectags st;

    struct OpenFileInfo * fileStack;
    char lbuf[10*BUFSIZ];
    char *lbufPtr;
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

    int force;
    int anyarch;

    int timeCheck;

    struct Source * sources;
    int numSources;
    int noSource;

    char * sourceRpmName;
    unsigned char * sourcePkgId;
    Header sourceHeader;
    rpmfi sourceCpioList;

    rpmMacroContext macros;

    StringBuf prep;		/*!< %prep scriptlet. */
    StringBuf build;		/*!< %build scriptlet. */
    StringBuf install;		/*!< %install scriptlet. */
    StringBuf check;		/*!< %check scriptlet. */
    StringBuf clean;		/*!< %clean scriptlet. */

    Package packages;		/*!< Package list. */
};

/** \ingroup rpmbuild
 * The structure used to store values for a package.
 */
struct Package_s {
    Header header;
    rpmds ds;			/*!< Requires: N = EVR */
    rpmfi cpioList;

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

    StringBuf specialDoc;
    char *specialDocDir;

    struct TriggerFileEntry * triggerFiles;

    StringBuf fileFile;
    StringBuf fileList;		/* If NULL, package will not be written */
    StringBuf policyList;

    Package next;
};

/** \ingroup rpmbuild
 * Destroy Spec structure.
 * @param spec		spec file control structure
 * @return		NULL always
 */
rpmSpec freeSpec(rpmSpec spec);

/** \ingroup rpmbuild
 * Function to query spec file(s).
 * @param ts		transaction set
 * @param qva		parsed query/verify options
 * @param arg		query argument
 * @return		0 on success, else no. of failures
 */
int rpmspecQuery(rpmts ts, QVA_t qva, const char * arg);

#ifdef __cplusplus
}
#endif

#endif /* _H_SPEC_ */
