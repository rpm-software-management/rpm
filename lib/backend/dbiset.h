#ifndef _DBISET_H
#define _DBISET_H

#include <rpm/rpmutil.h>

/* A single item from an index database (i.e. the "data returned"). */
struct dbiIndexItem {
    unsigned int hdrNum;		/*!< header instance in db */
    unsigned int tagNum;		/*!< tag index in header */
};

/* Items retrieved from the index database.*/
typedef struct dbiIndexSet_s {
    struct dbiIndexItem * recs;		/*!< array of records */
    unsigned int count;			/*!< number of records */
    size_t alloced;			/*!< alloced size */
} * dbiIndexSet;

#ifdef __cplusplus
extern "C" {
#endif

/* Reserve space for at least nrecs new records */
RPM_GNUC_INTERNAL
void dbiIndexSetGrow(dbiIndexSet set, unsigned int nrecs);

/* Sort an index set */
RPM_GNUC_INTERNAL
void dbiIndexSetSort(dbiIndexSet set);

/**
 * Append element(s) to set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to append to set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sortset	should resulting set be sorted?
 * @return		0 success, 1 failure (bad args)
 */
RPM_GNUC_INTERNAL
int dbiIndexSetAppend(dbiIndexSet set, const void * recs,
		      int nrecs, size_t recsize, int sortset);

/**
 * Remove element(s) from set of index database items.
 * @param set		set of index database items
 * @param recs		array of items to remove from set
 * @param nrecs		number of items
 * @param recsize	size of an array item
 * @param sorted	array is already sorted?
 * @return		0 success, 1 failure (no items found)
 */
RPM_GNUC_INTERNAL
int dbiIndexSetPrune(dbiIndexSet set, void * recs, int nrecs,
		     size_t recsize, int sorted);

/* Count items in index database set. */
RPM_GNUC_INTERNAL
unsigned int dbiIndexSetCount(dbiIndexSet set);

/* Return record offset of header from element in index database set. */
RPM_GNUC_INTERNAL
unsigned int dbiIndexRecordOffset(dbiIndexSet set, int recno);

/* Return file index from element in index database set. */
RPM_GNUC_INTERNAL
unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, int recno);

/* Destroy set of index database items */
RPM_GNUC_INTERNAL
dbiIndexSet dbiIndexSetFree(dbiIndexSet set);

#ifdef __cplusplus
}
#endif
#endif
