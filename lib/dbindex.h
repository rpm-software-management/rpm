#ifndef H_DBINDEX
#define H_DBINDEX

/* this will break if sizeof(int) != 4 */

#include <db.h>

typedef struct {
    unsigned int recOffset;
    unsigned int fileNumber;
} dbiIndexRecord;

typedef struct {
    dbiIndexRecord * recs;
    int count;
} dbiIndexSet;

typedef struct {
    DB * db;
    char * indexname;
} dbiIndex;

dbiIndex * dbiOpenIndex(char * filename, int flags, int perms);
void dbiCloseIndex(dbiIndex * dbi);
void dbiSyncIndex(dbiIndex * dbi);
int dbiSearchIndex(dbiIndex * dbi, char * str, dbiIndexSet * set);
   /* -1 error, 0 success, 1 not found */
int dbiUpdateIndex(dbiIndex * dbi, char * str, dbiIndexSet * set);
   /* 0 on success */
int dbiAppendIndexRecord(dbiIndexSet * set, dbiIndexRecord rec);
   /* 0 on success - should never fail */
int dbiRemoveIndexRecord(dbiIndexSet * set, dbiIndexRecord rec);
   /* 0 on success - fails if rec is not found */
dbiIndexSet dbiCreateIndexRecord(void);
void dbiFreeIndexRecord(dbiIndexSet set);

#endif
