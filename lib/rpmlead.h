#ifndef _H_RPMLEAD
#define _H_RPMLEAD

#define RPMLEAD_BINARY 0
#define RPMLEAD_SOURCE 1

#define RPMLEAD_MAGIC0 0xed
#define RPMLEAD_MAGIC1 0xab
#define RPMLEAD_MAGIC2 0xee
#define RPMLEAD_MAGIC3 0xdb

struct rpmlead {
    char magic[4];
    char major, minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
} ;

#endif
