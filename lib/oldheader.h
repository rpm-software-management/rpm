#ifndef H_OLDHEADER
#define H_OLDHEADER

#include <stdio.h>

struct oldrpmFileInfo {
    char * path;
    int state;
    unsigned short mode;
    unsigned short uid;
    unsigned short gid;
    unsigned short rdev;
    unsigned long size;
    unsigned long mtime;
    char md5[32];
    char * linkto;
    int isconf;
    int isdoc;
} ;

struct oldrpmHeader {
    unsigned short type, cpu;
    unsigned int size;
    unsigned int os;
    unsigned int iconLength;

    char * name, * version, * release, * group;
    char * icon;

    unsigned int specLength;
    char * spec;
} ;

struct oldrpmHeaderSpec {
    char * description;
    char * vendor;
    char * distribution;
    char * buildHost;
    char * copyright;
    char * prein, * postin, * preun, * postun;

    int buildTime;

    int fileCount;
    struct oldrpmFileInfo * files;
} ;

#ifdef __cplusplus
extern "C" {
#endif

void oldrpmfileFromSpecLine(char * str, struct oldrpmFileInfo * fi);
void oldrpmfileFromInfoLine(char * path, char * state, char * str,
			struct oldrpmFileInfo * fi);
void oldrpmfileFree(struct oldrpmFileInfo * fi);
char * oldrpmfileToInfoStr(struct oldrpmFileInfo * fi);

char * oldhdrReadFromStream(FD_t fd, struct oldrpmHeader * header);
char * oldhdrReadFromFile(char * filename, struct oldrpmHeader * header);
char * oldhdrParseSpec(struct oldrpmHeader * header, struct oldrpmHeaderSpec * spec);
void   oldhdrFree(struct oldrpmHeader * header);
void   oldhdrSpecFree(struct oldrpmHeaderSpec * spec);

#ifdef __cplusplus
}
#endif

#endif	/* H_OLDHEADER */
