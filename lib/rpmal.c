/** \ingroup rpmdep
 * \file lib/rpmal.c
 */

#include "system.h"

#include <unordered_map>

#include <rpm/rpmte.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmstrpool.h>

#include "rpmal.h"
#include "misc.h"
#include "rpmte_internal.h"
#include "rpmds_internal.h"
#include "rpmfi_internal.h"
#include "rpmts_internal.h"

#include "debug.h"

typedef int rpmalNum;

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
    rpmte p;                    /*!< transaction member */
    rpmds provides;		/*!< Provides: dependencies. */
    rpmds obsoletes;		/*!< Obsoletes: dependencies. */
    rpmfiles fi;		/*!< File info set. */
};

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
typedef struct availableIndexEntry_s {
    rpmalNum pkgNum;	        /*!< Containing package index. */
    unsigned int entryIx;	/*!< Dependency index. */
} * availableIndexEntry;

typedef struct availableIndexFileEntry_s {
    rpmsid dirName;
    rpmalNum pkgNum;	        /*!< Containing package index. */
    unsigned int entryIx;	/*!< Dependency index. */
} * availableIndexFileEntry;

using rpmalDepHash = std::unordered_multimap<rpmsid,availableIndexEntry_s>;
using rpmalFileHash = std::unordered_multimap<rpmsid,availableIndexFileEntry_s>;

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
struct rpmal_s {
    rpmstrPool pool;		/*!< String pool */
    std::vector<availablePackage_s> list;/*!< Set of packages. */
    rpmalDepHash *providesHash;
    rpmalDepHash *obsoletesHash;
    rpmalFileHash *fileHash;
    rpmtransFlags tsflags;	/*!< Transaction control flags. */
    rpm_color_t tscolor;	/*!< Transaction color. */
    rpm_color_t prefcolor;	/*!< Transaction preferred color. */
    fingerPrintCache fpc;
};

/**
 * Destroy available item index.
 * @param al		available list
 */
static void rpmalFreeIndex(rpmal al)
{
    delete al->providesHash;
    delete al->obsoletesHash;
    delete al->fileHash;
    al->fpc = fpCacheFree(al->fpc);
}

rpmal rpmalCreate(rpmts ts, int delta)
{
    rpmal al = new rpmal_s {};

    al->pool = rpmstrPoolLink(rpmtsPool(ts));
    al->tsflags = rpmtsFlags(ts);
    al->tscolor = rpmtsColor(ts);
    al->prefcolor = rpmtsPrefColor(ts);

    return al;
}

rpmal rpmalFree(rpmal al)
{
    if (al == NULL)
	return NULL;

    for (auto & alp : al->list) {
	rpmdsFree(alp.obsoletes);
	rpmdsFree(alp.provides);
	rpmfilesFree(alp.fi);
    }
    al->pool = rpmstrPoolFree(al->pool);

    rpmalFreeIndex(al);
    delete al;
    return NULL;
}

void rpmalDel(rpmal al, rpmte p)
{
    if (al == NULL)
	return;		/* XXX can't happen */

    // XXX use a search for self provide
    for (auto & alp : al->list) {
	// do not actually delete, just set p to NULL
	// and later filter that out of the results
	if (alp.p == p) {
	    alp.p = NULL;
	    break;
	}
    }
}

static void rpmalAddFiles(rpmal al, rpmalNum pkgNum, rpmfiles fi)
{
    struct availableIndexFileEntry_s fileEntry;
    int fc = rpmfilesFC(fi);
    rpm_color_t ficolor;
    int skipdoc = (al->tsflags & RPMTRANS_FLAG_NODOCS);
    int skipconf = (al->tsflags & RPMTRANS_FLAG_NOCONFIGS);

    fileEntry.pkgNum = pkgNum;

    for (int i = 0; i < fc; i++) {
	/* Ignore colored provides not in our rainbow. */
        ficolor = rpmfilesFColor(fi, i);
        if (al->tscolor && ficolor && !(al->tscolor & ficolor))
            continue;

	/* Ignore files that wont be installed */
	if (skipdoc && (rpmfilesFFlags(fi, i) & RPMFILE_DOC))
	    continue;
	if (skipconf && (rpmfilesFFlags(fi, i) & RPMFILE_CONFIG))
	    continue;

	fileEntry.dirName = rpmfilesDNId(fi, rpmfilesDI(fi, i));
	fileEntry.entryIx = i;

	al->fileHash->insert({rpmfilesBNId(fi, i), fileEntry});
    }
}

static void rpmalAddProvides(rpmal al, rpmalNum pkgNum, rpmds provides)
{
    struct availableIndexEntry_s indexEntry;
    rpm_color_t dscolor;
    int skipconf = (al->tsflags & RPMTRANS_FLAG_NOCONFIGS);
    int dc = rpmdsCount(provides);

    indexEntry.pkgNum = pkgNum;

    for (int i = 0; i < dc; i++) {
        /* Ignore colored provides not in our rainbow. */
        dscolor = rpmdsColorIndex(provides, i);
        if (al->tscolor && dscolor && !(al->tscolor & dscolor))
            continue;

	/* Ignore config() provides if the files wont be installed */
	if (skipconf & (rpmdsFlagsIndex(provides, i) & RPMSENSE_CONFIG))
	    continue;

	indexEntry.entryIx = i;;
	al->providesHash->insert({rpmdsNIdIndex(provides, i), indexEntry});
    }
}

static void rpmalAddObsoletes(rpmal al, rpmalNum pkgNum, rpmds obsoletes)
{
    struct availableIndexEntry_s indexEntry;
    rpm_color_t dscolor;
    int dc = rpmdsCount(obsoletes);

    indexEntry.pkgNum = pkgNum;

    for (int i = 0; i < dc; i++) {
	/* Obsoletes shouldn't be colored but just in case... */
        dscolor = rpmdsColorIndex(obsoletes, i);
        if (al->tscolor && dscolor && !(al->tscolor & dscolor))
            continue;

	indexEntry.entryIx = i;;
	al->obsoletesHash->insert({rpmdsNIdIndex(obsoletes, i), indexEntry});
    }
}

void rpmalAdd(rpmal al, rpmte p)
{
    /* Source packages don't provide anything to depsolving */
    if (rpmteIsSource(p))
	return;

    rpmalNum pkgNum = al->list.size();

    availablePackage_s alp = {
	.p = p,
	.provides = rpmdsLink(rpmteDS(p, RPMTAG_PROVIDENAME)),
	.obsoletes = rpmdsLink(rpmteDS(p, RPMTAG_OBSOLETENAME)),
	.fi = rpmteFiles(p),
    };

    al->list.push_back(alp);

    /* Try to be lazy as delayed hash creation is cheaper */
    if (al->providesHash != NULL)
	rpmalAddProvides(al, pkgNum, alp.provides);
    if (al->obsoletesHash != NULL)
	rpmalAddObsoletes(al, pkgNum, alp.obsoletes);
    if (al->fileHash != NULL)
	rpmalAddFiles(al, pkgNum, alp.fi);
}

static void rpmalMakeFileIndex(rpmal al)
{
    int fileCnt = 0;

    for (auto const & alp : al->list) {
	if (alp.fi != NULL)
	    fileCnt += rpmfilesFC(alp.fi);
    }
    al->fileHash = new rpmalFileHash(fileCnt/4+128);
    int i = 0;
    for (auto const & alp : al->list) {
	rpmalAddFiles(al, i++, alp.fi);
    }
}

static void rpmalMakeProvidesIndex(rpmal al)
{
    int providesCnt = 0;

    for (auto const & alp : al->list) {
	providesCnt += rpmdsCount(alp.provides);
    }

    al->providesHash = new rpmalDepHash(providesCnt/4+128);

    int i = 0;
    for (auto const & alp : al->list) {
	rpmalAddProvides(al, i++, alp.provides);
    }
}

static void rpmalMakeObsoletesIndex(rpmal al)
{
    int obsoletesCnt = 0;

    for (auto const & alp : al->list) {
	obsoletesCnt += rpmdsCount(alp.obsoletes);
    }

    al->obsoletesHash = new rpmalDepHash(obsoletesCnt/4+128);

    int i = 0;
    for (auto const & alp : al->list) {
	rpmalAddObsoletes(al, i++, alp.obsoletes);
    }
}

std::vector<rpmte> rpmalAllObsoletes(rpmal al, rpmds ds)
{
    std::vector<rpmte> ret;
    rpmsid nameId;

    if (al == NULL || ds == NULL || (nameId = rpmdsNId(ds)) == 0)
	return ret;

    if (al->obsoletesHash == NULL)
	rpmalMakeObsoletesIndex(al);

    auto range = al->obsoletesHash->equal_range(nameId);
    for (auto it = range.first; it != range.second; ++it) {
	auto & alp = al->list[it->second.pkgNum];
	if (alp.p == NULL) // deleted
	    continue;

	int rc = rpmdsCompareIndex(alp.obsoletes, it->second.entryIx,
			       ds, rpmdsIx(ds));

	if (rc) {
	    rpmdsNotify(ds, "(added obsolete)", 0);
	    ret.push_back(alp.p);
	}
    }

    return ret;
}

static std::vector<rpmte> rpmalAllFileSatisfiesDepend(const rpmal al, const char *fileName, const rpmds filterds)
{
    const char *slash; 
    std::vector<rpmte> ret;

    if (al == NULL || fileName == NULL || *fileName != '/')
	return ret;

    /* Split path into dirname and basename components for lookup */
    if ((slash = strrchr(fileName, '/')) != NULL) {
	size_t bnStart = (slash - fileName) + 1;
	rpmsid baseName;

	if (al->fileHash == NULL)
	    rpmalMakeFileIndex(al);

	baseName = rpmstrPoolId(al->pool, fileName + bnStart, 0);
	if (!baseName)
	    return ret;	/* no match possible */

	auto range = al->fileHash->equal_range(baseName);
	if (range.first != range.second) {
	    fingerPrint * fp = NULL;
	    rpmsid dirName = rpmstrPoolIdn(al->pool, fileName, bnStart, 1);

	    for (auto it = range.first; it != range.second; ++it) {
		auto & alp = al->list[it->second.pkgNum];
		if (alp.p == NULL) /* deleted */
		    continue;
		/* ignore self-conflicts/obsoletes */
		if (filterds && rpmteDS(alp.p, rpmdsTagN(filterds)) == filterds)
		    continue;
		if (it->second.dirName != dirName) {
		    /* if the directory is different check the fingerprints */
		    if (!al->fpc)
			al->fpc = fpCacheCreate(1001, al->pool);
		    if (!fp)
			fpLookupId(al->fpc, dirName, baseName, &fp);
		    if (!fpLookupEqualsId(al->fpc, fp, it->second.dirName, baseName))
			continue;
		}
		ret.push_back(alp.p);
	    }
	    _free(fp);
	}
    }

    return ret;
}

std::vector<rpmte> rpmalAllSatisfiesDepend(const rpmal al, const rpmds ds)
{
    std::vector<rpmte> ret;
    rpmsid nameId;
    const char *name;
    int obsolete;
    rpmTagVal dtag;
    rpmds filterds = NULL;

    int rc;

    if (al == NULL || ds == NULL || (nameId = rpmdsNId(ds)) == 0)
	return ret;

    dtag = rpmdsTagN(ds);
    obsolete = (dtag == RPMTAG_OBSOLETENAME);
    if (dtag == RPMTAG_OBSOLETENAME || dtag == RPMTAG_CONFLICTNAME)
	filterds = ds;
    name = rpmstrPoolStr(al->pool, nameId);
    if (!obsolete && *name == '/') {
	/* First, look for files "contained" in package ... */
	ret = rpmalAllFileSatisfiesDepend(al, name, filterds);
	if (!ret.empty()) {
	    rpmdsNotify(ds, "(added files)", 0);
	    return ret;
	}
	/* ... then, look for files "provided" by package. */
    }

    if (al->providesHash == NULL)
	rpmalMakeProvidesIndex(al);

    auto range = al->providesHash->equal_range(nameId);
    for (auto it = range.first; it != range.second; ++it) {
	auto & alp = al->list[it->second.pkgNum];
	if (alp.p == NULL) /* deleted */
	    continue;
	/* ignore self-conflicts/obsoletes */
	if (filterds && rpmteDS(alp.p, rpmdsTagN(filterds)) == filterds)
	    continue;
	int ix = it->second.entryIx;

	if (obsolete) {
	    /* Obsoletes are on package NEVR only */
	    rpmds thisds;
	    if (!rstreq(rpmdsNIndex(alp.provides, ix), rpmteN(alp.p)))
		continue;
	    thisds = rpmteDS(alp.p, RPMTAG_NAME);
	    rc = rpmdsCompareIndex(thisds, rpmdsIx(thisds), ds, rpmdsIx(ds));
	} else {
	    rc = rpmdsCompareIndex(alp.provides, ix, ds, rpmdsIx(ds));
	}

	if (rc)
	    ret.push_back(alp.p);
    }

    if (!ret.empty()) {
	rpmdsNotify(ds, "(added provide)", 0);
    }

    return ret;
}

rpmte
rpmalSatisfiesDepend(const rpmal al, const rpmte te, const rpmds ds)
{
    auto const providers = rpmalAllSatisfiesDepend(al, ds);
    rpmte best = NULL;
    int bestscore = 0;

    if (!providers.empty()) {
	rpm_color_t dscolor = rpmdsColor(ds);
	for (rpmte const p : providers) {
	    int score = 0;

	    /*
	     * For colored dependencies, prefer a matching colored provider.
	     * Otherwise prefer provider of ts preferred color.
	     */
	    if (al->tscolor) {
		rpm_color_t tecolor = rpmteColor(p);
		if (dscolor) {
		    if (dscolor == tecolor) score += 2;
		} else if (al->prefcolor) {
		    if (al->prefcolor == tecolor) score += 2;
		}
	    }

	    /* Being self-provided is a bonus */
	    if (p == te)
		score += 1;

	    if (score > bestscore) {
		bestscore = score;
		best = p;
	    }
	}
	/* if not decided by now, just pick first match */
	if (!best)
	    best = providers[0];
    }
    return best;
}

unsigned int
rpmalLookupTE(const rpmal al, const rpmte te)
{
    rpmalNum pkgNum = (unsigned int)-1;
    for (auto it = al->list.begin(); it != al->list.end(); ++it) {
	if (it->p == te) {
	    pkgNum = it - al->list.begin();
	    break;
	}
    }
    return pkgNum;
}

