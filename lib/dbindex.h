#ifndef H_DBINDEX
#define H_DBINDEX

#ifdef HAVE_DB1_DB_H
#include <db1/db.h>
#else
#ifdef HAVE_DB_185_H
#include <db_185.h>
#else
#include <db.h>
#endif
#endif

/* this will break if sizeof(int) != 4 */

typedef /*@abstract@*/ struct {
    unsigned int recOffset;
    unsigned int fileNumber;
} dbiIndexRecord;

typedef /*@abstract@*/ struct {
    dbiIndexRecord * recs;
    int count;
} dbiIndexSet;

typedef /*@abstract@*/ struct {
    DB * db;
    const char * indexname;
} dbiIndex;

#ifdef __cplusplus
extern "C" {
#endif

/*@only@*/dbiIndex * dbiOpenIndex(const char * filename, int flags, int perms, DBTYPE type);
void dbiCloseIndex(/*@only@*/dbiIndex * dbi);
void dbiSyncIndex(dbiIndex * dbi);
int dbiSearchIndex(dbiIndex * dbi, const char * str, /*@out@*/dbiIndexSet * set);
   /* -1 error, 0 success, 1 not found */
int dbiUpdateIndex(dbiIndex * dbi, const char * str, dbiIndexSet * set);
   /* 0 on success */
int dbiAppendIndexRecord(/*@out@*/dbiIndexSet * set, dbiIndexRecord rec);
   /* 0 on success - should never fail */
int dbiRemoveIndexRecord(dbiIndexSet * set, dbiIndexRecord rec);
   /* 0 on success - fails if rec is not found */
dbiIndexSet dbiCreateIndexRecord(void);
void dbiFreeIndexRecord(dbiIndexSet set);
int dbiGetFirstKey(dbiIndex * dbi, /*@out@*/const char ** key);

extern unsigned int dbiIndexSetCount(dbiIndexSet set);

/* structure return */
extern dbiIndexRecord dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber);

extern unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno);

extern unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno);

#ifdef __cplusplus
}
#endif

#endif	/* H_DBINDEX */
