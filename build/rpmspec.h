#ifndef _H_SPEC_
#define _H_SPEC_

typedef struct SpecStruct *Spec;
#include "rpmmacro.h"

#if 0
struct ReqProvTrigger {
    int flags;
    char *name;
    char *version;
    int index;      /* Only used for triggers */
    struct ReqProvTrigger *next;
};
#endif

struct TriggerFileEntry {
    int index;
    char *fileName;
    char *script;
    char *prog;
    struct TriggerFileEntry *next;
};

#define RPMBUILD_ISSOURCE     1
#define RPMBUILD_ISPATCH     (1 << 1)
#define RPMBUILD_ISICON      (1 << 2)
#define RPMBUILD_ISNO        (1 << 3)

#define RPMBUILD_DEFAULT_LANG "C"

struct Source {
    char *fullSource;
    char *source;     /* Pointer into fullSource */
    int flags;
    int num;
    struct Source *next;
};

struct ReadLevelEntry {
    int reading;
    struct ReadLevelEntry *next;
};

struct OpenFileInfo {
    char *fileName;
    FILE *file;
    int lineNum;
    char readBuf[BUFSIZ];
    char *readPtr;
    struct OpenFileInfo *next;
};

struct SpecStruct {
    char *specFile;
    char *sourceRpmName;

    struct OpenFileInfo *fileStack;
    char line[BUFSIZ];
    int lineNum;

    struct ReadLevelEntry *readStack;

    Header buildRestrictions;
    struct SpecStruct **buildArchitectureSpecs;
    char ** buildArchitectures;
    int buildArchitectureCount;
    int inBuildArchitectures;

    int gotBuildRoot;
    char *buildRoot;
    char *buildSubdir;

    char *passPhrase;
    int timeCheck;
    char *cookie;

    struct Source *sources;
    int numSources;
    int noSource;

    Header sourceHeader;
    int sourceCpioCount;
    struct cpioFileMapping *sourceCpioList;

    struct MacroContext *macros;

    int autoReq;
    int autoProv;

    StringBuf prep;
    StringBuf build;
    StringBuf install;
    StringBuf clean;

    struct PackageStruct *packages;
};

struct PackageStruct {
    Header header;

    int cpioCount;
    struct cpioFileMapping *cpioList;

    struct Source *icon;

    int autoReqProv;

    char *preInFile;
    char *postInFile;
    char *preUnFile;
    char *postUnFile;
    char *verifyFile;

    StringBuf specialDoc;

#if 0
    struct ReqProvTrigger *triggers;
    char *triggerScripts;
#endif

    struct TriggerFileEntry *triggerFiles;

    char *fileFile;
    StringBuf fileList; /* If NULL, package will not be written */

    struct PackageStruct *next;
};

typedef struct PackageStruct *Package;

#ifdef __cplusplus
extern "C" {
#endif

Spec newSpec(void);
void freeSpec(Spec spec);

struct OpenFileInfo * newOpenFileInfo(void);

int addSource(Spec spec, Package pkg, char *field, int tag);
int parseNoSource(Spec spec, char *field, int tag);

#ifdef __cplusplus
}
#endif

#endif /* _H_SPEC_ */
