#ifndef _DBISET_H
#define _DBISET_H

#include <rpm/rpmutil.h>

/* A single item from an index database (i.e. the "data returned"). */
typedef struct dbiIndexItem_s {
    unsigned int hdrNum;		/*!< header instance in db */
    unsigned int tagNum;		/*!< tag index in header */

    bool operator < (const dbiIndexItem_s & other) const;
    bool operator == (const dbiIndexItem_s & other) const;
} * dbiIndexItem;

typedef struct dbiIndexSet_s * dbiIndexSet;

/* Create an empty index set, optionally with sizehint reservation for recs */
dbiIndexSet dbiIndexSetNew(unsigned int sizehint);

/* Reserve space for at least nrecs new records */
void dbiIndexSetGrow(dbiIndexSet set, unsigned int nrecs);

void dbiIndexSetClear(dbiIndexSet set);

/* Sort an index set */
void dbiIndexSetSort(dbiIndexSet set);

/* Uniq an index set */
void dbiIndexSetUniq(dbiIndexSet set, int sorted);

/* Append an index set to another */
int dbiIndexSetAppendSet(dbiIndexSet set, dbiIndexSet oset, int sortset);

/**
  * Append a single element to a set of index database items.
  * @param set          set of index database items
  * @param hdrNum       header instance in db
  * @param tagNum       tag index in header
  * @param sortset      should resulting set be sorted?
  * @return             0 success, 1 failure (bad args)
  */
int dbiIndexSetAppendOne(dbiIndexSet set, unsigned int hdrNum,
			 unsigned int tagNum, int sortset);

/**
 * Remove an index set from another.
 * The sets are sorted as a side-effect if sorted is 0.
 * @param set          set of index database items
 * @param oset         set of entries that should be removed
 * @param sorted       are set and oset already sorted?
 * @return             0 success, 1 failure (no items found)
 */
int dbiIndexSetPruneSet(dbiIndexSet set, dbiIndexSet oset, int sorted);

/**
 * Filter (intersect) an index set with another.
 * The sets are sorted as a side-effect if sorted is 0.
 * @param set          set of index database items
 * @param oset         set of entries that should be intersected
 * @param sorted       are set and oset already sorted?
 * @return             0 success, 1 failure (no items removed)
 */
int dbiIndexSetFilterSet(dbiIndexSet set, dbiIndexSet oset, int sorted);

/* Count items in index database set. */
unsigned int dbiIndexSetCount(dbiIndexSet set);

/* Return record offset of header from element in index database set. */
unsigned int dbiIndexRecordOffset(dbiIndexSet set, unsigned int recno);

/* Return file index from element in index database set. */
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, unsigned int recno);

/* Destroy set of index database items */
dbiIndexSet dbiIndexSetFree(dbiIndexSet set);
#endif
