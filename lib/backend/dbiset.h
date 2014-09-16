#ifndef _DBISET_H
#define _DBISET_H

#include <rpm/rpmutil.h>

/* A single item from an index database (i.e. the "data returned"). */
typedef struct dbiIndexItem_s {
    unsigned int hdrNum;		/*!< header instance in db */
    unsigned int tagNum;		/*!< tag index in header */
} * dbiIndexItem;

/* Items retrieved from the index database.*/
typedef struct dbiIndexSet_s {
    dbiIndexItem recs;			/*!< array of records */
    unsigned int count;			/*!< number of records */
    size_t alloced;			/*!< alloced size */
} * dbiIndexSet;

#ifdef __cplusplus
extern "C" {
#endif

/* Create an empty index set, optionally with sizehint reservation for recs */
RPM_GNUC_INTERNAL
dbiIndexSet dbiIndexSetNew(unsigned int sizehint);

/* Reserve space for at least nrecs new records */
RPM_GNUC_INTERNAL
void dbiIndexSetGrow(dbiIndexSet set, unsigned int nrecs);

/* Sort an index set */
RPM_GNUC_INTERNAL
void dbiIndexSetSort(dbiIndexSet set);

/* Uniq an index set */
RPM_GNUC_INTERNAL
void dbiIndexSetUniq(dbiIndexSet set, int sorted);

/* Append an index set to another */
RPM_GNUC_INTERNAL
int dbiIndexSetAppendSet(dbiIndexSet set, dbiIndexSet oset, int sortset);

/**
 * Append element(s) to set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to append to set
 * @param nrecs		number of items
 * @param sortset	should resulting set be sorted?
 * @return		0 success, 1 failure (bad args)
 */
RPM_GNUC_INTERNAL
int dbiIndexSetAppend(dbiIndexSet set, dbiIndexItem recs,
		      unsigned int nrecs, int sortset);

/**
 * Remove element(s) from set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to remove from set
 * @param nrecs		number of items
 * @param sorted	array is already sorted?
 * @return		0 success, 1 failure (no items found)
 */
RPM_GNUC_INTERNAL
int dbiIndexSetPrune(dbiIndexSet set, dbiIndexItem recs,
		     unsigned int nrecs, int sorted);

/* Count items in index database set. */
RPM_GNUC_INTERNAL
unsigned int dbiIndexSetCount(dbiIndexSet set);

/* Return record offset of header from element in index database set. */
RPM_GNUC_INTERNAL
unsigned int dbiIndexRecordOffset(dbiIndexSet set, unsigned int recno);

/* Return file index from element in index database set. */
RPM_GNUC_INTERNAL
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, unsigned int recno);

/* Destroy set of index database items */
RPM_GNUC_INTERNAL
dbiIndexSet dbiIndexSetFree(dbiIndexSet set);

#ifdef __cplusplus
}
#endif
#endif
