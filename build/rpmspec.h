#ifndef _H_SPEC_
#define _H_SPEC_

/** \ingroup rpmbuild
 * \file build/rpmspec.h
 *  The Spec and Package data structures used during build.
 */

/** \ingroup rpmbuild
 */
typedef struct SpecStruct *Spec;

#include "rpmmacro.h"

/** \ingroup rpmbuild
 */
struct TriggerFileEntry {
    int index;
/*@only@*/ char *fileName;
/*@only@*/ char *script;
/*@only@*/ char *prog;
/*@owned@*/ struct TriggerFileEntry *next;
};

#define RPMBUILD_ISSOURCE     1
#define RPMBUILD_ISPATCH     (1 << 1)
#define RPMBUILD_ISICON      (1 << 2)
#define RPMBUILD_ISNO        (1 << 3)

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
typedef struct ReadLevelEntry {
    int reading;
/*@dependent@*/ struct ReadLevelEntry * next;
} RLE_t;

/** \ingroup rpmbuild
 */
typedef struct OpenFileInfo {
/*@only@*/ const char * fileName;
    FD_t fd;
    int lineNum;
    char readBuf[BUFSIZ];
/*@dependent@*/ char *readPtr;
/*@owned@*/ struct OpenFileInfo * next;
} OFI_t;

/** \ingroup rpmbuild
 */
struct spectag {
    int t_tag;
    int t_startx;
    int t_nlines;
/*@only@*/ const char * t_lang;
/*@only@*/ const char * t_msgid;
};

/** \ingroup rpmbuild
 */
struct spectags {
/*@owned@*/ struct spectag *st_t;
    int st_nalloc;
    int st_ntags;
};

/** \ingroup rpmbuild
 */
struct speclines {
/*@only@*/ char **sl_lines;
    int sl_nalloc;
    int sl_nlines;
};

/** \ingroup rpmbuild
 * The structure used to store values parsed from a spec file.
 */
struct SpecStruct {
/*@only@*/ const char * specFile;	/*!< Name of the spec file. */
/*@only@*/ const char * sourceRpmName;
/*@only@*/ const char * buildRootURL;
/*@only@*/ const char * buildSubdir;
/*@only@*/ const char * rootURL;

/*@owned@*/ /*@null@*/ struct speclines * sl;
/*@owned@*/ /*@null@*/ struct spectags * st;

/*@owned@*/ struct OpenFileInfo * fileStack;
    char lbuf[4*BUFSIZ];
    char nextpeekc;
/*@dependent@*/ char * nextline;
/*@dependent@*/ char * line;
    int lineNum;

/*@owned@*/ struct ReadLevelEntry * readStack;

/*@refcounted@*/ Header buildRestrictions;
/*@owned@*/ /*@null@*/ struct SpecStruct ** buildArchitectureSpecs;
/*@only@*/ /*@null@*/ const char ** buildArchitectures;
    int buildArchitectureCount;
    int inBuildArchitectures;

    int force;
    int anyarch;

    int gotBuildRootURL;

/*@null@*/ char * passPhrase;
    int timeCheck;
/*@null@*/ const char * cookie;

/*@owned@*/ struct Source * sources;
    int numSources;
    int noSource;

/*@refcounted@*/ Header sourceHeader;
/*@owned@*/ void * sourceCpioList;

/*@dependent@*/ /*@null@*/ MacroContext macros;

/*@only@*/ StringBuf prep;		/*!< %prep scriptlet. */
/*@only@*/ StringBuf build;		/*!< %build scriptlet. */
/*@only@*/ StringBuf install;		/*!< %install scriptlet. */
/*@only@*/ StringBuf clean;		/*!< %clean scriptlet. */

/*@owned@*/ struct PackageStruct * packages;	/*!< Package list. */
};

/** \ingroup rpmbuild
 * The structure used to store values for a package.
 */
struct PackageStruct {
/*@refcounted@*/ Header header;

/*@owned@*/ void * cpioList;

/*@owned@*/ struct Source * icon;

    int autoReq;
    int autoProv;

/*@only@*/ const char * preInFile;	/*!< %pre scriptlet. */
/*@only@*/ const char * postInFile;	/*!< %post scriptlet. */
/*@only@*/ const char * preUnFile;	/*!< %preun scriptlet. */
/*@only@*/ const char * postUnFile;	/*!< %postun scriptlet. */
/*@only@*/ const char * verifyFile;	/*!< %verifyscript scriptlet. */

/*@only@*/ StringBuf specialDoc;

/*@only@*/ struct TriggerFileEntry * triggerFiles;

/*@only@*/ const char * fileFile;
/*@only@*/ StringBuf fileList; /* If NULL, package will not be written */

/*@dependent@*/ struct PackageStruct * next;
};

/** \ingroup rpmbuild
 */
typedef struct PackageStruct * Package;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 * Create and initialize Spec structure.
 */
/*@only@*/ Spec newSpec(void)	/*@*/;

/** \ingroup rpmbuild
 * Destroy Spec structure.
 * @param spec		spec file control structure
 */
void freeSpec(/*@only@*/ Spec spec)
	/*@modifies spec @*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 */
extern void (*freeSpecVec) (Spec spec)	/* XXX FIXME */
	/*@modifies spec @*/;

/** \ingroup rpmbuild
 */
struct OpenFileInfo * newOpenFileInfo(void)	/*@*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 */
struct spectag * stashSt(Spec spec, Header h, int tag, const char * lang)
	/*@modifies spec->st @*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 */
int addSource(Spec spec, Package pkg, const char * field, int tag)
	/*@modifies spec->sources, spec->numSources,
		spec->st,
		pkg->icon @*/;

/** \ingroup rpmbuild
 * @param spec		spec file control structure
 */
int parseNoSource(Spec spec, const char * field, int tag)
	/*@modifies nothing @*/;

#ifdef __cplusplus
}
#endif

#endif /* _H_SPEC_ */
