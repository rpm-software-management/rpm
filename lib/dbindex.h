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
    char * indexname;
} dbiIndex;

#ifdef __cplusplus
extern "C" {
#endif

dbiIndex * dbiOpenIndex(char * filename, int flags, int perms, DBTYPE type);
void dbiCloseIndex(dbiIndex * dbi);
void dbiSyncIndex(dbiIndex * dbi);
int dbiSearchIndex(dbiIndex * dbi, const char * str, dbiIndexSet * set);
   /* -1 error, 0 success, 1 not found */
int dbiUpdateIndex(dbiIndex * dbi, const char * str, dbiIndexSet * set);
   /* 0 on success */
int dbiAppendIndexRecord(dbiIndexSet * set, dbiIndexRecord rec);
   /* 0 on success - should never fail */
int dbiRemoveIndexRecord(dbiIndexSet * set, dbiIndexRecord rec);
   /* 0 on success - fails if rec is not found */
dbiIndexSet dbiCreateIndexRecord(void);
void dbiFreeIndexRecord(dbiIndexSet set);
int dbiGetFirstKey(dbiIndex * dbi, const char ** key);

extern inline int dbiIndexSetCount(dbiIndexSet set);
extern inline int dbiIndexSetCount(dbiIndexSet set) {
    return set.count;
}

/* structure return */
extern inline dbiIndexRecord dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber);
extern inline dbiIndexRecord dbiReturnIndexRecordInstance(unsigned int recOffset, unsigned int fileNumber) {
    dbiIndexRecord rec;
    rec.recOffset = recOffset;
    rec.fileNumber = fileNumber;
    return rec;
}

extern inline unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno);
extern inline unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno) {
    return set.recs[recno].recOffset;
}

extern inline unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno);
extern inline unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno) {
    return set.recs[recno].fileNumber;
}

#ifdef __cplusplus
}
#endif

#endif	/* H_DBINDEX */
