#ifndef H_DBINDEX
#define H_DBINDEX

/* this will break if sizeof(int) != 4 */

#include <db.h>

typedef struct {
    unsigned int recOffset;
    unsigned int fileNumber;
} dbIndexRecord;

typedef struct {
    dbIndexRecord * recs;
    int count;
} dbIndexSet;

typedef struct {
    DB * db;
    char * indexname;
} dbIndex;

dbIndex * openDBIndex(char * filename, int flags, int perms);
void closeDBIndex(dbIndex * dbi);
int searchDBIndex(dbIndex * dbi, char * str, dbIndexSet * set);
   /* -1 error, 0 success, 1 not found */
int updateDBIndex(dbIndex * dbi, char * str, dbIndexSet * set);
   /* 0 on success */
int appendDBIndexRecord(dbIndexSet * set, dbIndexRecord rec);
   /* 0 on success - should never fail */
int removeDBIndexRecord(dbIndexSet * set, dbIndexRecord rec);
   /* 0 on success - fails if rec is not found */
dbIndexSet createDBIndexRecord(void);
void freeDBIndexRecord(dbIndexSet set);

#endif
