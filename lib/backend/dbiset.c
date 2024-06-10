#include "system.h"
#include <string.h>
#include <stdlib.h>
#include "dbiset.h"
#include "debug.h"

dbiIndexSet dbiIndexSetNew(unsigned int sizehint)
{
    dbiIndexSet set = (dbiIndexSet)xcalloc(1, sizeof(*set));
    if (sizehint > 0)
	dbiIndexSetGrow(set, sizehint);
    return set;
}

/* 
 * Ensure sufficient memory for nrecs of new records in dbiIndexSet.
 * Allocate in power of two sizes to avoid memory fragmentation, so
 * realloc is not always needed.
 */
void dbiIndexSetGrow(dbiIndexSet set, unsigned int nrecs)
{
    size_t need = (set->count + nrecs) * sizeof(*(set->recs));
    size_t alloced = set->alloced ? set->alloced : 1 << 4;

    while (alloced < need)
	alloced <<= 1;

    if (alloced != set->alloced) {
	set->recs = xrealloc(set->recs, alloced);
	set->alloced = alloced;
    }
}

void dbiIndexSetClear(dbiIndexSet set)
{
    if (set)
	set->count = 0;
}

static int hdrNumCmp(const void * one, const void * two)
{
    const struct dbiIndexItem_s *a = (const struct dbiIndexItem_s *)one;
    const struct dbiIndexItem_s *b = (const struct dbiIndexItem_s *)two;
    if (a->hdrNum - b->hdrNum != 0)
	return a->hdrNum - b->hdrNum;
    return a->tagNum - b->tagNum;
}

void dbiIndexSetSort(dbiIndexSet set)
{
    /*
     * mergesort is much (~10x with lots of identical basenames) faster
     * than pure quicksort, but glibc uses msort_with_tmp() on stack.
     */
    if (set && set->recs && set->count > 1) {
#ifdef HAVE_MERGESORT
	mergesort(set->recs, set->count, sizeof(*set->recs), hdrNumCmp);
#else
	qsort(set->recs, set->count, sizeof(*set->recs), hdrNumCmp);
#endif
    }
}

void dbiIndexSetUniq(dbiIndexSet set, int sorted)
{
    unsigned int from;
    unsigned int to = 0;
    unsigned int num = set->count;

    if (set->count < 2)
	return;

    if (!sorted)
	dbiIndexSetSort(set);

    for (from = 0; from < num; from++) {
	if (from > 0 && set->recs[from - 1].hdrNum == set->recs[from].hdrNum) {
	    set->count--;
	    continue;
	}
	if (from != to)
	    set->recs[to] = set->recs[from]; /* structure assignment */
	to++;
    }
}

int dbiIndexSetAppendSet(dbiIndexSet set, dbiIndexSet oset, int sortset)
{
    if (set == NULL || oset == NULL)
	return 1;

    if (oset->count) {
	dbiIndexSetGrow(set, oset->count);
	memcpy(set->recs + set->count, oset->recs, oset->count * sizeof(*(set->recs)));
	set->count += oset->count;
    }
    
    if (sortset && set->count > 1)
	dbiIndexSetSort(set);

    return 0;
}

int dbiIndexSetAppendOne(dbiIndexSet set, unsigned int hdrNum,
			 unsigned int tagNum, int sortset)
{
    if (set == NULL)
	return 1;
    dbiIndexSetGrow(set, 1);

    set->recs[set->count].hdrNum = hdrNum;
    set->recs[set->count].tagNum = tagNum;
    set->count += 1;

    if (sortset && set->count > 1)
	dbiIndexSetSort(set);

    return 0;
}

/* op 0 to prune, 1 to filter */
static int dbiIndexSetPrune(dbiIndexSet set, dbiIndexSet oset, int op)
{
    unsigned int from;
    unsigned int to = 0;
    unsigned int num = set->count;
    unsigned int numCopied = 0;
    size_t recsize = sizeof(*oset->recs);

    for (from = 0; from < num; from++) {
	if ((bsearch(&set->recs[from], oset->recs, oset->count, recsize, hdrNumCmp) != NULL) == op) {
	    set->count--;
	    continue;
	}
	if (from != to)
	    set->recs[to] = set->recs[from]; /* structure assignment */
	to++;
	numCopied++;
    }
    return (numCopied == num);
}

int dbiIndexSetPruneSet(dbiIndexSet set, dbiIndexSet oset, int sorted)
{

    if (set->count == 0 || oset->count == 0)
	return 1;

    if (oset->count > 1 && !sorted)
	dbiIndexSetSort(oset);

    return dbiIndexSetPrune(set, oset, 1);
}

int dbiIndexSetFilterSet(dbiIndexSet set, dbiIndexSet oset, int sorted)
{
    if (set->count == 0 || oset->count == 0) {
	unsigned int num = set->count;
	dbiIndexSetClear(set);
	return num ? 0 : 1;
    }

    if (oset->count > 1 && !sorted)
	dbiIndexSetSort(oset);

    return dbiIndexSetPrune(set, oset, 0);
}

unsigned int dbiIndexSetCount(dbiIndexSet set)
{
    return (set != NULL) ? set->count : 0;
}

unsigned int dbiIndexRecordOffset(dbiIndexSet set, unsigned int recno)
{
    return set->recs[recno].hdrNum;
}

unsigned int dbiIndexRecordFileNumber(dbiIndexSet set, unsigned int recno)
{
    return set->recs[recno].tagNum;
}

dbiIndexSet dbiIndexSetFree(dbiIndexSet set)
{
    if (set) {
	free(set->recs);
	memset(set, 0, sizeof(*set)); /* trash and burn */
	free(set);
    }
    return NULL;
}
