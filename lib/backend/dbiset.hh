#ifndef _DBISET_H
#define _DBISET_H

#include <rpm/rpmutil.h>

/* A single item from an index database (i.e. the "data returned"). */
typedef struct RPM_GNUC_INTERNAL dbiIndexItem_s {
    unsigned int hdrNum;		/*!< header instance in db */
    unsigned int tagNum;		/*!< tag index in header */

    bool operator < (const dbiIndexItem_s & other) const;
    bool operator == (const dbiIndexItem_s & other) const;
} * dbiIndexItem;

typedef struct dbiIndexSet_s * dbiIndexSet;

/* Create an empty index set, optionally with sizehint reservation for recs */
RPM_GNUC_INTERNAL
dbiIndexSet dbiIndexSetNew(unsigned int sizehint);

/* Reserve space for at least nrecs new records */
RPM_GNUC_INTERNAL
void dbiIndexSetGrow(dbiIndexSet set, unsigned int nrecs);

RPM_GNUC_INTERNAL
void dbiIndexSetClear(dbiIndexSet set);

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
  * Append a single element to a set of index database items.
  * @param set          set of index database items
  * @param hdrNum       header instance in db
  * @param tagNum       tag index in header
  * @param sortset      should resulting set be sorted?
  * @return             0 success, 1 failure (bad args)
  */
RPM_GNUC_INTERNAL
int dbiIndexSetAppendOne(dbiIndexSet set, unsigned int hdrNum,
			 unsigned int tagNum, int sortset);

/**
 * Remove an index set from another.
 * The sets are sorted as a side-effect if sorted is 0.
 * @param set          set of index database items
 * @param oset         set of entries that should be removed
 * @param sorted       are set and oset is already sorted?
 * @return             0 success, 1 failure (no items found)
 */
RPM_GNUC_INTERNAL
int dbiIndexSetPruneSet(dbiIndexSet set, dbiIndexSet oset, int sorted);

/**
 * Filter (intersect) an index set with another.
 * The sets are sorted as a side-effect if sorted is 0.
 * @param set          set of index database items
 * @param oset         set of entries that should be intersected
 * @param sorted       are set and oset are already sorted?
 * @return             0 success, 1 failure (no items removed)
 */
RPM_GNUC_INTERNAL
int dbiIndexSetFilterSet(dbiIndexSet set, dbiIndexSet oset, int sorted);

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
#endif
