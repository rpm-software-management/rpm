#ifndef _H_SPEC_
#define _H_SPEC_

/** \ingroup rpmbuild
 * \file build/rpmspec.h
 *  The Spec and Package data structures used during build.
 */

/** \ingroup rpmbuild
 */
typedef struct Package_s * Package;

/** \ingroup rpmbuild
 */
struct TriggerFileEntry {
    int index;
/*@only@*/ char * fileName;
/*@only@*/ char * script;
/*@only@*/ char * prog;
/*@owned@*/ struct TriggerFileEntry * next;
};

#define RPMBUILD_ISSOURCE	(1 << 0)
#define RPMBUILD_ISPATCH	(1 << 1)
#define RPMBUILD_ISICON		(1 << 2)
#define RPMBUILD_ISNO		(1 << 3)

#define RPMBUILD_DEFAULT_LANG "C"

/** \ingroup rpmbuild
 */
struct Source {
/*@owned@*/ char * fullSource;
/*@dependent@*/ char * source;     /* Pointer into fullSource */
    int flags;
    int num;
/*@owned@*/ struct Source * next;
};

/** \ingroup rpmbuild
 */
/*@-typeuse@*/
typedef struct ReadLevelEntry {
    int reading;
/*@dependent@*/
    struct ReadLevelEntry * next;
} RLE_t;
/*@=typeuse@*/

/** \ingroup rpmbuild
 */
typedef struct OpenFileInfo {
/*@only@*/ const char * fileName;
    FD_t fd;
    int lineNum;
    char readBuf[BUFSIZ];
/*@dependent@*/
    char * readPtr;
/*@owned@*/
    struct OpenFileInfo * next;
} OFI_t;

/** \ingroup rpmbuild
 */
typedef struct spectag_s {
    int t_tag;
    int t_startx;
    int t_nlines;
/*@only@*/ const char * t_lang;
/*@only@*/ const char * t_msgid;
} * spectag;

/** \ingroup rpmbuild
 */
typedef struct spectags_s {
/*@owned@*/ spectag st_t;
    int st_nalloc;
    int st_ntags;
} * spectags;

/** \ingroup rpmbuild
 */
typedef struct speclines_s {
/*@only@*/ char **sl_lines;
    int sl_nalloc;
    int sl_nlines;
} * speclines;

/** \ingroup rpmbuild
 * The structure used to store values parsed from a spec file.
 */
struct Spec_s {
/*@only@*/
    const char * specFile;	/*!< Name of the spec file. */
/*@only@*/
    const char * buildRootURL;
/*@only@*/
    const char * buildSubdir;
/*@only@*/
    const char * rootURL;

/*@owned@*/ /*@null@*/
    speclines sl;
/*@owned@*/ /*@null@*/
    spectags st;

/*@owned@*/
    struct OpenFileInfo * fileStack;
    char lbuf[4*BUFSIZ];
    char nextpeekc;
/*@dependent@*/
    char * nextline;
/*@dependent@*/
    char * line;
    int lineNum;

/*@owned@*/
    struct ReadLevelEntry * readStack;

/*@refcounted@*/
    Header buildRestrictions;
/*@owned@*/ /*@null@*/
    Spec * BASpecs;
/*@only@*/ /*@null@*/
    const char ** BANames;
    int BACount;
    int recursing;		/*!< parse is recursive? */

    int force;
    int anyarch;

    int gotBuildRootURL;

/*@null@*/
    char * passPhrase;
    int timeCheck;
/*@null@*/
    const char * cookie;

/*@owned@*/
    struct Source * sources;
    int numSources;
    int noSource;

/*@only@*/
    const char * sourceRpmName;
/*@only@*/
    unsigned char * sourcePkgId;
/*@refcounted@*/
    Header sourceHeader;
/*@refcounted@*/
    rpmfi sourceCpioList;

/*@dependent@*/ /*@null@*/
    MacroContext macros;

/*@only@*/
    StringBuf prep;		/*!< %prep scriptlet. */
/*@only@*/
    StringBuf build;		/*!< %build scriptlet. */
/*@only@*/
    StringBuf install;		/*!< %install scriptlet. */
/*@only@*/
    StringBuf check;		/*!< %check scriptlet. */
/*@only@*/
    StringBuf clean;		/*!< %clean scriptlet. */

/*@owned@*/
    Package packages;		/*!< Package list. */
};

/** \ingroup rpmbuild
 * The structure used to store values for a package.
 */
struct Package_s {
/*@refcounted@*/
    Header header;
/*@refcounted@*/
    rpmds ds;			/*!< Requires: N = EVR */
/*@refcounted@*/
    rpmfi cpioList;

/*@owned@*/
    struct Source * icon;

    int autoReq;
    int autoProv;

/*@only@*/
    const char * preInFile;	/*!< %pre scriptlet. */
/*@only@*/
    const char * postInFile;	/*!< %post scriptlet. */
/*@only@*/
    const char * preUnFile;	/*!< %preun scriptlet. */
/*@only@*/
    const char * postUnFile;	/*!< %postun scriptlet. */
/*@only@*/
    const char * verifyFile;	/*!< %verifyscript scriptlet. */

/*@only@*/
    StringBuf specialDoc;

/*@only@*/
    struct TriggerFileEntry * triggerFiles;

/*@only@*/
    const char * fileFile;
/*@only@*/
    StringBuf fileList;		/* If NULL, package will not be written */

/*@dependent@*/
    Package next;
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 * Create and initialize Spec structure.
 * @return spec		spec file control structure
 */
/*@only@*/ Spec newSpec(void)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies rpmGlobalMacroContext @*/;

/** \ingroup rpmbuild
 * Destroy Spec structure.
 * @param spec		spec file control structure
 * @return		NULL always
 */
/*@null@*/ Spec freeSpec(/*@only@*/ /*@null@*/ Spec spec)
	/*@globals fileSystem, internalState @*/
	/*@modifies spec, fileSystem, internalState @*/;

/** \ingroup rpmbuild
 * Function to query spec file(s).
 * @param ts		transaction set
 * @param qva		parsed query/verify options
 * @param arg		query argument
 * @return		0 on success, else no. of failures
 */
int rpmspecQuery(rpmts ts, QVA_t qva, const char * arg)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies ts, qva, rpmGlobalMacroContext,
		fileSystem, internalState @*/;

/** \ingroup rpmbuild
 */
struct OpenFileInfo * newOpenFileInfo(void)
	/*@*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 * @param h
 * @param tag
 * @param lang
 */
spectag stashSt(Spec spec, Header h, int tag, const char * lang)
	/*@modifies spec->st @*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 * @param pkg		package control
 * @param field
 * @param tag
 */
int addSource(Spec spec, Package pkg, const char * field, int tag)
	/*@globals rpmGlobalMacroContext, h_errno @*/
	/*@modifies spec->sources, spec->numSources,
		spec->st, spec->macros,
		pkg->icon,
		rpmGlobalMacroContext @*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 * @param field
 * @param tag
 */
int parseNoSource(Spec spec, const char * field, int tag)
	/*@modifies nothing @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_SPEC_ */
