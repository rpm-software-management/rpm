#ifndef _SPEC_H_
#define _SPEC_H_

#include "header.h"
#include "stringbuf.h"
#include "macro.h"

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

struct SpecStruct {
    char *specFile;
    char *sourceRpmName;

    FILE *file;
    char readBuf[BUFSIZ];
    char *readPtr;
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

    char *docDir;

    char *passPhrase;
    int timeCheck;
    char *cookie;

    struct Source *sources;
    int numSources;
    int noSource;

    Header sourceHeader;
    int sourceCpioCount;
    struct cpioFileMapping *sourceCpioList;
    
    struct MacroContext macros;

    int autoReqProv;
    
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

typedef struct SpecStruct *Spec;
typedef struct PackageStruct *Package;

Spec newSpec(void);
void freeSpec(Spec spec);

int addSource(Spec spec, Package pkg, char *field, int tag);
char *getSource(Spec spec, int num, int flag);
char *getFullSource(Spec spec, int num, int flag);
void freeSources(Spec spec);
int parseNoSource(Spec spec, char *field, int tag);

#endif /* _SPEC_H_ */
