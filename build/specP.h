#ifndef _SPECP_H_
#define _SPECP_H_

#include "spec.h"
#include "header.h"
#include "stringbuf.h"

typedef struct sources {
    char *fullSource;
    char *source;
    int ispatch;
    int num;
    struct sources *next;
} *Sources;

struct ReqProv {
    int flags;
    char *name;
    char *version;
    struct ReqProv *next;
};

struct SpecRec {
    char *name;      /* package base name */
    char *specfile;

    int numSources;
    int numPatches;
    Sources sources;

    int numNoSource;
    int numNoPatch;
    int_32 *noSource;
    int_32 *noPatch;

    StringBuf prep;
    StringBuf build;
    StringBuf install;
    StringBuf doc;
    StringBuf clean;

    char *prefix;
    char *buildprefix;
    
    struct PackageRec *packages;
    /* The first package record is the "main" package and contains
     * the bulk of the preamble information.  Subsequent package
     * records "inherit" from the main record.  Note that the
     * "main" package may be, in pre-rpm-2.0 terms, a "subpackage".
     */
};

struct PackageRec {
    char *subname;   /* If both of these are NULL, then this is          */
    char *newname;   /* the main package.  subname concats with name     */
    Header header;
    char *icon;
    int files;       /* If -1, package has no files, and won't be written */
    char *fileFile;
    StringBuf filelist;
    StringBuf doc;   /* Used to buffer up %doc lines until fully parsed */
    int numReq;
    int numProv;
    int numConflict;
    struct ReqProv *reqprov;
    struct PackageRec *next;
};

#endif _SPECP_H_
