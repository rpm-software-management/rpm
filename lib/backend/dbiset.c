#include "system.h"

#include <vector>
#include <algorithm>

#include <string.h>
#include <stdlib.h>
#include "dbiset.h"
#include "debug.h"

using std::vector;

bool dbiIndexItem_s::operator < (const dbiIndexItem_s & other) const
{
    if (hdrNum == other.hdrNum)
	return tagNum < other.tagNum;
    return hdrNum < other.hdrNum;
}

bool dbiIndexItem_s::operator == (const dbiIndexItem_s & other) const
{
    return (hdrNum == other.hdrNum && tagNum == other.tagNum);
}

/* Items retrieved from the index database.*/
struct dbiIndexSet_s {
    vector<dbiIndexItem_s> recs;	/*!< array of records */
};

dbiIndexSet dbiIndexSetNew(unsigned int sizehint)
{
    dbiIndexSet set = new dbiIndexSet_s {};
    if (sizehint > 0)
	dbiIndexSetGrow(set, sizehint);
    return set;
}

void dbiIndexSetGrow(dbiIndexSet set, unsigned int nrecs)
{
    set->recs.reserve(nrecs);
}

void dbiIndexSetClear(dbiIndexSet set)
{
    if (set)
	set->recs.clear();
}

void dbiIndexSetSort(dbiIndexSet set)
{
    if (set)
	std::sort(set->recs.begin(), set->recs.end());
}

static bool hdrNumCmp(const dbiIndexItem_s & a, const dbiIndexItem_s & b)
{
    return (a.hdrNum == b.hdrNum);
}

void dbiIndexSetUniq(dbiIndexSet set, int sorted)
{
    if (set->recs.size() < 2)
	return;

    if (!sorted)
	dbiIndexSetSort(set);

    auto it = std::unique(set->recs.begin(), set->recs.end(), hdrNumCmp);
    set->recs.resize(std::distance(set->recs.begin(), it));
}

int dbiIndexSetAppendSet(dbiIndexSet set, dbiIndexSet oset, int sortset)
{
    if (set == NULL || oset == NULL)
	return 1;

    set->recs.insert(set->recs.end(), oset->recs.begin(), oset->recs.end());
    
    if (sortset && set->recs.size() > 1)
	dbiIndexSetSort(set);

    return 0;
}

int dbiIndexSetAppendOne(dbiIndexSet set, unsigned int hdrNum,
			 unsigned int tagNum, int sortset)
{
    if (set == NULL)
	return 1;

    set->recs.emplace_back() = { hdrNum, tagNum };

    if (sortset && set->recs.size() > 1)
	dbiIndexSetSort(set);

    return 0;
}

int dbiIndexSetPruneSet(dbiIndexSet set, dbiIndexSet oset, int sorted)
{
    if (set->recs.empty() || oset == NULL || oset->recs.empty())
	return 1;

    if (oset->recs.size() > 1 && !sorted)
	dbiIndexSetSort(oset);

    vector<dbiIndexItem_s> diff;
    std::set_difference(set->recs.begin(), set->recs.end(),
			  oset->recs.begin(), oset->recs.end(),
			  std::back_inserter(diff));
    set->recs = diff;
    return 0;
}

int dbiIndexSetFilterSet(dbiIndexSet set, dbiIndexSet oset, int sorted)
{
    if (set->recs.empty() || oset == NULL || oset->recs.empty()) {
	unsigned int num = set->recs.size();
	dbiIndexSetClear(set);
	return num ? 0 : 1;
    }

    if (oset->recs.size() > 1 && !sorted)
	dbiIndexSetSort(oset);

    vector<dbiIndexItem_s> isect;
    std::set_intersection(set->recs.begin(), set->recs.end(),
		      oset->recs.begin(), oset->recs.end(),
		      std::back_inserter(isect));
    set->recs = isect;
    return 0;
}

unsigned int dbiIndexSetCount(dbiIndexSet set)
{
    return (set != NULL) ? set->recs.size() : 0;
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
	delete set;
    }
    return NULL;
}
