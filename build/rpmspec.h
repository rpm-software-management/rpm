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

/** \ingroup rpmbuild
 */
struct TriggerFileEntry {
    int index;
    char * fileName;
    char * script;
    char * prog;
    struct TriggerFileEntry * next;
};

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

/** \ingroup rpmbuild
 */
typedef struct ReadLevelEntry {
    int reading;
    struct ReadLevelEntry * next;
} RLE_t;

/** \ingroup rpmbuild
 */
typedef struct OpenFileInfo {
    char * fileName;
    FILE *fp;
    int lineNum;
    char readBuf[BUFSIZ];
    char * readPtr;
    struct OpenFileInfo * next;
} OFI_t;

/** \ingroup rpmbuild
 */
typedef struct spectag_s {
    int t_tag;
    int t_startx;
    int t_nlines;
    char * t_lang;
    char * t_msgid;
} * spectag;

/** \ingroup rpmbuild
 */
typedef struct spectags_s {
    spectag st_t;
    int st_nalloc;
    int st_ntags;
} * spectags;

/** \ingroup rpmbuild
 */
typedef struct speclines_s {
    char **sl_lines;
    int sl_nalloc;
    int sl_nlines;
} * speclines;

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

    char * passPhrase;
    int timeCheck;
    char * cookie;

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

    Package next;
};

/** \ingroup rpmbuild
 * Create and initialize rpmSpec structure.
 * @return spec		spec file control structure
 */
rpmSpec newSpec(void);

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

/** \ingroup rpmbuild
 */
struct OpenFileInfo * newOpenFileInfo(void);

/** \ingroup rpmbuild
 * stashSt.
 * @param spec		spec file control structure
 * @param h		header
 * @param tag		tag
 * @param lang		locale
 */
spectag stashSt(rpmSpec spec, Header h, rpmTag tag, const char * lang);

/** \ingroup rpmbuild
 * addSource.
 * @param spec		spec file control structure
 * @param pkg		package control
 * @param field		field to parse
 * @param tag		tag
 */
int addSource(rpmSpec spec, Package pkg, const char * field, rpmTag tag);

/** \ingroup rpmbuild
 * parseNoSource.
 * @param spec		spec file control structure
 * @param field		field to parse
 * @param tag		tag
 */
int parseNoSource(rpmSpec spec, const char * field, rpmTag tag);

#ifdef __cplusplus
}
#endif

#endif /* _H_SPEC_ */
