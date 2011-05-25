/** \ingroup rpmdep
 * \file lib/rpmte.c
 * Routine(s) to handle an "rpmte"  transaction element.
 */
#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* RPM_MACHTABLE_* */
#include <rpm/rpmmacro.h>
#include <rpm/rpmds.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlog.h>

#include "lib/rpmplugins.h"
#include "lib/rpmte_internal.h"

#include "debug.h"

/** \ingroup rpmte
 * A single package instance to be installed/removed atomically.
 */
struct rpmte_s {
    rpmElementType type;	/*!< Package disposition (installed/removed). */

    Header h;			/*!< Package header. */
    char * NEVR;		/*!< Package name-version-release. */
    char * NEVRA;		/*!< Package name-version-release.arch. */
    char * name;		/*!< Name: */
    char * epoch;
    char * version;		/*!< Version: */
    char * release;		/*!< Release: */
    char * arch;		/*!< Architecture hint. */
    char * os;			/*!< Operating system hint. */
    int isSource;		/*!< (TR_ADDED) source rpm? */

    rpmte depends;              /*!< Package updated by this package (ERASE te) */
    rpmte parent;		/*!< Parent transaction element. */
    unsigned int db_instance;	/*!< Database instance (of removed pkgs) */
    tsortInfo tsi;		/*!< Dependency ordering chains. */

    rpmds thisds;		/*!< This package's provided NEVR. */
    rpmds provides;		/*!< Provides: dependencies. */
    rpmds requires;		/*!< Requires: dependencies. */
    rpmds conflicts;		/*!< Conflicts: dependencies. */
    rpmds obsoletes;		/*!< Obsoletes: dependencies. */
    rpmds order;		/*!< Order: dependencies. */
    rpmfi fi;			/*!< File information. */
    rpmps probs;		/*!< Problems (relocations) */
    rpmts ts;			/*!< Parent transaction */

    rpm_color_t color;		/*!< Color bit(s) from package dependencies. */
    rpm_loff_t pkgFileSize;	/*!< No. of bytes in package file (approx). */
    unsigned int headerSize;	/*!< No. of bytes in package header */

    fnpyKey key;		/*!< (TR_ADDED) Retrieval key. */
    rpmRelocation * relocs;	/*!< (TR_ADDED) Payload file relocations. */
    int nrelocs;		/*!< (TR_ADDED) No. of relocations. */
    FD_t fd;			/*!< (TR_ADDED) Payload file descriptor. */

#define RPMTE_HAVE_PRETRANS	(1 << 0)
#define RPMTE_HAVE_POSTTRANS	(1 << 1)
    int transscripts;		/*!< pre/posttrans script existence */
    int failed;			/*!< (parent) install/erase failed */

    rpmfs fs;

    ARGV_t lastInCollectionsAny;	/*!< list of collections this te is the last to be installed or removed */
    ARGV_t lastInCollectionsAdd;	/*!< list of collections this te is the last to be only installed */
    ARGV_t firstInCollectionsRemove;	/*!< list of collections this te is the first to be only removed */
    ARGV_t collections;			/*!< list of collections */
};

/* forward declarations */
static void rpmteColorDS(rpmte te, rpmTag tag);
static int rpmteClose(rpmte te, int reset_fi);

void rpmteCleanDS(rpmte te)
{
    te->thisds = rpmdsFree(te->thisds);
    te->provides = rpmdsFree(te->provides);
    te->requires = rpmdsFree(te->requires);
    te->conflicts = rpmdsFree(te->conflicts);
    te->obsoletes = rpmdsFree(te->obsoletes);
    te->order = rpmdsFree(te->order);
}

static rpmfi getFI(rpmte p, Header h)
{
    rpmfiFlags fiflags;
    fiflags = (p->type == TR_ADDED) ? (RPMFI_NOHEADER | RPMFI_FLAGS_INSTALL) :
				      (RPMFI_NOHEADER | RPMFI_FLAGS_ERASE);

    /* relocate stuff in header if necessary */
    if (rpmteType(p) == TR_ADDED && rpmfsFC(p->fs) > 0) {
	if (!headerIsSource(h) && !headerIsEntry(h, RPMTAG_ORIGBASENAMES)) {
	    rpmRelocateFileList(p->relocs, p->nrelocs, p->fs, h);
	}
    }
    return rpmfiNew(NULL, h, RPMTAG_BASENAMES, fiflags);
}

/* stupid bubble sort, but it's probably faster here */
static void sortRelocs(rpmRelocation *relocations, int numRelocations)
{
    for (int i = 0; i < numRelocations; i++) {
	int madeSwap = 0;
	for (int j = 1; j < numRelocations; j++) {
	    rpmRelocation tmpReloc;
	    if (relocations[j - 1].oldPath == NULL || /* XXX can't happen */
		relocations[j    ].oldPath == NULL || /* XXX can't happen */
		strcmp(relocations[j - 1].oldPath, relocations[j].oldPath) <= 0)
		continue;
	    tmpReloc = relocations[j - 1];
	    relocations[j - 1] = relocations[j];
	    relocations[j] = tmpReloc;
	    madeSwap = 1;
	}
	if (!madeSwap) break;
    }
}

static char * stripTrailingChar(char * s, char c)
{
    char * t;
    for (t = s + strlen(s) - 1; *t == c && t >= s; t--)
	*t = '\0';
    return s;
}

static void buildRelocs(rpmte p, Header h, rpmRelocation *relocs)
{
    int i;
    struct rpmtd_s validRelocs;

    for (rpmRelocation *r = relocs; r->oldPath || r->newPath; r++)
	p->nrelocs++;

    headerGet(h, RPMTAG_PREFIXES, &validRelocs, HEADERGET_MINMEM);
    p->relocs = xmalloc(sizeof(*p->relocs) * (p->nrelocs+1));

    /* Build sorted relocation list from raw relocations. */
    for (i = 0; i < p->nrelocs; i++) {
	char * t;

	/*
	 * Default relocations (oldPath == NULL) are handled in the UI,
	 * not rpmlib.
	 */
	if (relocs[i].oldPath == NULL) continue; /* XXX can't happen */

	/* FIXME: Trailing /'s will confuse us greatly. Internal ones will 
	   too, but those are more trouble to fix up. :-( */
	t = xstrdup(relocs[i].oldPath);
	p->relocs[i].oldPath = (t[0] == '/' && t[1] == '\0')
	    ? t
	    : stripTrailingChar(t, '/');

	/* An old path w/o a new path is valid, and indicates exclusion */
	if (relocs[i].newPath) {
	    int valid = 0;
	    const char *validprefix;

	    t = xstrdup(relocs[i].newPath);
	    p->relocs[i].newPath = (t[0] == '/' && t[1] == '\0')
		? t
		: stripTrailingChar(t, '/');

	   	/* FIX:  relocations[i].oldPath == NULL */
	    /* Verify that the relocation's old path is in the header. */
	    rpmtdInit(&validRelocs);
	    while ((validprefix = rpmtdNextString(&validRelocs))) {
		if (rstreq(validprefix, p->relocs[i].oldPath)) {
		    valid = 1;
		    break;
		}
	    }

	    if (!valid) {
		rpmteAddProblem(p, RPMPROB_BADRELOCATE, NULL,
				p->relocs[i].oldPath, 0);
	    }
	} else {
	    p->relocs[i].newPath = NULL;
	}
    }
    p->relocs[i].oldPath = NULL;
    p->relocs[i].newPath = NULL;
    sortRelocs(p->relocs, p->nrelocs);
    
    rpmtdFreeData(&validRelocs);
}

/**
 * Initialize transaction element data from header.
 * @param p		transaction element
 * @param h		header
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 */
static void addTE(rpmte p, Header h, fnpyKey key, rpmRelocation * relocs)
{
    struct rpmtd_s colls;

    p->name = headerGetAsString(h, RPMTAG_NAME);
    p->version = headerGetAsString(h, RPMTAG_VERSION);
    p->release = headerGetAsString(h, RPMTAG_RELEASE);

    p->epoch = headerGetAsString(h, RPMTAG_EPOCH);

    p->arch = headerGetAsString(h, RPMTAG_ARCH);
    p->os = headerGetAsString(h, RPMTAG_OS);

    p->isSource = headerIsSource(h);
    
    p->NEVR = headerGetAsString(h, RPMTAG_NEVR);
    p->NEVRA = headerGetAsString(h, RPMTAG_NEVRA);

    p->nrelocs = 0;
    p->relocs = NULL;
    if (relocs != NULL)
	buildRelocs(p, h, relocs);

    p->db_instance = headerGetInstance(h);
    p->key = key;
    p->fd = NULL;

    p->pkgFileSize = 0;
    p->headerSize = headerSizeof(h, HEADER_MAGIC_NO);

    p->thisds = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    p->provides = rpmdsNew(h, RPMTAG_PROVIDENAME, 0);
    p->requires = rpmdsNew(h, RPMTAG_REQUIRENAME, 0);
    p->conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, 0);
    p->obsoletes = rpmdsNew(h, RPMTAG_OBSOLETENAME, 0);
    p->order = rpmdsNew(h, RPMTAG_ORDERNAME, 0);

    p->fs = rpmfsNew(h, p->type);
    p->fi = getFI(p, h);

    /* See if we have pre/posttrans scripts. */
    p->transscripts |= (headerIsEntry(h, RPMTAG_PRETRANS) &&
			 headerIsEntry(h, RPMTAG_PRETRANSPROG)) ?
			RPMTE_HAVE_PRETRANS : 0;
    p->transscripts |= (headerIsEntry(h, RPMTAG_POSTTRANS) &&
			 headerIsEntry(h, RPMTAG_POSTTRANSPROG)) ?
			RPMTE_HAVE_POSTTRANS : 0;

    p->lastInCollectionsAny = NULL;
    p->lastInCollectionsAdd = NULL;
    p->firstInCollectionsRemove = NULL;
    p->collections = NULL;
    if (headerGet(h, RPMTAG_COLLECTIONS, &colls, HEADERGET_MINMEM)) {
	const char *collname;
	while ((collname = rpmtdNextString(&colls))) {
	    argvAdd(&p->collections, collname);
	}
	argvSort(p->collections, NULL);
	rpmtdFreeData(&colls);
    }

    rpmteColorDS(p, RPMTAG_PROVIDENAME);
    rpmteColorDS(p, RPMTAG_REQUIRENAME);
    return;
}

rpmte rpmteFree(rpmte te)
{
    if (te != NULL) {
	if (te->relocs) {
	    for (int i = 0; i < te->nrelocs; i++) {
		free(te->relocs[i].oldPath);
		free(te->relocs[i].newPath);
	    }
	    free(te->relocs);
	}

	free(te->os);
	free(te->arch);
	free(te->epoch);
	free(te->name);
	free(te->version);
	free(te->release);
	free(te->NEVR);
	free(te->NEVRA);

	fdFree(te->fd);
	rpmfiFree(te->fi);
	headerFree(te->h);
	rpmfsFree(te->fs);
	rpmpsFree(te->probs);
	rpmteCleanDS(te);

	argvFree(te->collections);
	argvFree(te->lastInCollectionsAny);
	argvFree(te->lastInCollectionsAdd);
	argvFree(te->firstInCollectionsRemove);

	memset(te, 0, sizeof(*te));	/* XXX trash and burn */
	free(te);
    }
    return NULL;
}

rpmte rpmteNew(rpmts ts, Header h, rpmElementType type, fnpyKey key,
	       rpmRelocation * relocs)
{
    rpmte p = xcalloc(1, sizeof(*p));
    p->ts = ts;
    p->type = type;
    addTE(p, h, key, relocs);
    switch (type) {
    case TR_ADDED:
	p->pkgFileSize = headerGetNumber(h, RPMTAG_LONGSIGSIZE) + 96 + 256;
	break;
    case TR_REMOVED:
	/* nothing to do */
	break;
    }

    return p;
}

unsigned int rpmteDBInstance(rpmte te) 
{
    return (te != NULL ? te->db_instance : 0);
}

void rpmteSetDBInstance(rpmte te, unsigned int instance) 
{
    if (te != NULL) 
	te->db_instance = instance;
}

Header rpmteHeader(rpmte te)
{
    return (te != NULL && te->h != NULL ? headerLink(te->h) : NULL);
}

Header rpmteSetHeader(rpmte te, Header h)
{
    if (te != NULL)  {
	te->h = headerFree(te->h);
	if (h != NULL)
	    te->h = headerLink(h);
    }
    return NULL;
}

rpmElementType rpmteType(rpmte te)
{
    /* XXX returning negative for unsigned type */
    return (te != NULL ? te->type : -1);
}

const char * rpmteN(rpmte te)
{
    return (te != NULL ? te->name : NULL);
}

const char * rpmteE(rpmte te)
{
    return (te != NULL ? te->epoch : NULL);
}

const char * rpmteV(rpmte te)
{
    return (te != NULL ? te->version : NULL);
}

const char * rpmteR(rpmte te)
{
    return (te != NULL ? te->release : NULL);
}

const char * rpmteA(rpmte te)
{
    return (te != NULL ? te->arch : NULL);
}

const char * rpmteO(rpmte te)
{
    return (te != NULL ? te->os : NULL);
}

int rpmteIsSource(rpmte te)
{
    return (te != NULL ? te->isSource : 0);
}

rpm_color_t rpmteColor(rpmte te)
{
    return (te != NULL ? te->color : 0);
}

rpm_color_t rpmteSetColor(rpmte te, rpm_color_t color)
{
    rpm_color_t ocolor = 0;
    if (te != NULL) {
	ocolor = te->color;
	te->color = color;
    }
    return ocolor;
}

ARGV_const_t rpmteCollections(rpmte te)
{
    return (te != NULL) ? te->collections : NULL;
}

int rpmteHasCollection(rpmte te, const char *collname)
{
    return (argvSearch(rpmteCollections(te), collname, NULL) != NULL);
}

int rpmteAddToLastInCollectionAdd(rpmte te, const char *collname)
{
    if (te != NULL) {
	argvAdd(&te->lastInCollectionsAdd, collname);
	argvSort(te->lastInCollectionsAdd, NULL);
	return 0;
    }
    return -1;
}

int rpmteAddToLastInCollectionAny(rpmte te, const char *collname)
{
    if (te != NULL) {
	argvAdd(&te->lastInCollectionsAny, collname);
	argvSort(te->lastInCollectionsAny, NULL);
	return 0;
    }
    return -1;
}

int rpmteAddToFirstInCollectionRemove(rpmte te, const char *collname)
{
    if (te != NULL) {
	argvAdd(&te->firstInCollectionsRemove, collname);
	argvSort(te->firstInCollectionsRemove, NULL);
	return 0;
    }
    return -1;
}

rpm_loff_t rpmtePkgFileSize(rpmte te)
{
    return (te != NULL ? te->pkgFileSize : 0);
}

unsigned int rpmteHeaderSize(rpmte te) {
    return (te != NULL ? te->headerSize : 0);
}

rpmte rpmteParent(rpmte te)
{
    return (te != NULL ? te->parent : NULL);
}

rpmte rpmteSetParent(rpmte te, rpmte pte)
{
    rpmte opte = NULL;
    if (te != NULL) {
	opte = te->parent;
	te->parent = pte;
    }
    return opte;
}

tsortInfo rpmteTSI(rpmte te)
{
    return te->tsi;
}

void rpmteSetTSI(rpmte te, tsortInfo tsi)
{
    te->tsi = tsi;
}

void rpmteSetDependsOn(rpmte te, rpmte depends) {
    te->depends = depends;
}

rpmte rpmteDependsOn(rpmte te)
{
    return te->depends;
}

int rpmteDBOffset(rpmte te)
{
    return rpmteDBInstance(te);
}

const char * rpmteEVR(rpmte te)
{
    return (te != NULL ? te->NEVR + strlen(te->name) + 1 : NULL);
}

const char * rpmteNEVR(rpmte te)
{
    return (te != NULL ? te->NEVR : NULL);
}

const char * rpmteNEVRA(rpmte te)
{
    return (te != NULL ? te->NEVRA : NULL);
}

FD_t rpmteSetFd(rpmte te, FD_t fd)
{
    if (te != NULL)  {
	if (te->fd != NULL)
	    te->fd = fdFree(te->fd);
	if (fd != NULL)
	    te->fd = fdLink(fd);
    }
    return NULL;
}

fnpyKey rpmteKey(rpmte te)
{
    return (te != NULL ? te->key : NULL);
}

rpmds rpmteDS(rpmte te, rpmTagVal tag)
{
    if (te == NULL)
	return NULL;

    switch (tag) {
    case RPMTAG_NAME:		return te->thisds;
    case RPMTAG_PROVIDENAME:	return te->provides;
    case RPMTAG_REQUIRENAME:	return te->requires;
    case RPMTAG_CONFLICTNAME:	return te->conflicts;
    case RPMTAG_OBSOLETENAME:	return te->obsoletes;
    case RPMTAG_ORDERNAME:	return te->order;
    default:			break;
    }
    return NULL;
}

rpmfi rpmteSetFI(rpmte te, rpmfi fi)
{
    if (te != NULL)  {
	te->fi = rpmfiFree(te->fi);
	if (fi != NULL)
	    te->fi = rpmfiLink(fi);
    }
    return NULL;
}

rpmfi rpmteFI(rpmte te)
{
    if (te == NULL)
	return NULL;

    return te->fi; /* XXX take fi reference here? */
}

static void rpmteColorDS(rpmte te, rpmTag tag)
{
    rpmfi fi = rpmteFI(te);
    rpmds ds = rpmteDS(te, tag);
    char deptype = 'R';
    char mydt;
    const uint32_t * ddict;
    rpm_color_t * colors;
    rpm_color_t val;
    int Count;
    unsigned ix;
    int ndx, i;

    if (!(te && (Count = rpmdsCount(ds)) > 0 && rpmfiFC(fi) > 0))
	return;

    switch (tag) {
    default:
	return;
	break;
    case RPMTAG_PROVIDENAME:
	deptype = 'P';
	break;
    case RPMTAG_REQUIRENAME:
	deptype = 'R';
	break;
    }

    colors = xcalloc(Count, sizeof(*colors));

    /* Calculate dependency color. */
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	val = rpmfiFColor(fi);
	ddict = NULL;
	ndx = rpmfiFDepends(fi, &ddict);
	if (ddict != NULL)
	while (ndx-- > 0) {
	    ix = *ddict++;
	    mydt = ((ix >> 24) & 0xff);
	    if (mydt != deptype)
		continue;
	    ix &= 0x00ffffff;
assert (ix < Count);
	    colors[ix] |= val;
	}
    }

    /* Set color values in dependency set. */
    ds = rpmdsInit(ds);
    while ((i = rpmdsNext(ds)) >= 0) {
	val = colors[i];
	te->color |= val;
	(void) rpmdsSetColor(ds, val);
    }
    free(colors);
}

static Header rpmteDBHeader(rpmte te)
{
    Header h = NULL;
    rpmdbMatchIterator mi;

    mi = rpmtsInitIterator(te->ts, RPMDBI_PACKAGES,
			   &te->db_instance, sizeof(te->db_instance));
    /* iterator returns weak refs, grab hold of header */
    if ((h = rpmdbNextIterator(mi)))
	h = headerLink(h);
    mi = rpmdbFreeIterator(mi);
    return h;
}

static Header rpmteFDHeader(rpmte te)
{
    Header h = NULL;
    te->fd = rpmtsNotify(te->ts, te, RPMCALLBACK_INST_OPEN_FILE, 0, 0);
    if (te->fd != NULL) {
	rpmVSFlags ovsflags;
	rpmRC pkgrc;

	ovsflags = rpmtsSetVSFlags(te->ts,
				   rpmtsVSFlags(te->ts) | RPMVSF_NEEDPAYLOAD);
	pkgrc = rpmReadPackageFile(te->ts, te->fd, rpmteNEVRA(te), &h);
	rpmtsSetVSFlags(te->ts, ovsflags);
	switch (pkgrc) {
	default:
	    rpmteClose(te, 1);
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    break;
	}
    }
    return h;
}

static int rpmteOpen(rpmte te, int reload_fi)
{
    Header h = NULL;
    if (te == NULL || te->ts == NULL || rpmteFailed(te))
	goto exit;

    rpmteSetHeader(te, NULL);

    switch (rpmteType(te)) {
    case TR_ADDED:
	h = rpmteDBInstance(te) ? rpmteDBHeader(te) : rpmteFDHeader(te);
	break;
    case TR_REMOVED:
	h = rpmteDBHeader(te);
    	break;
    }
    if (h != NULL) {
	if (reload_fi) {
	    te->fi = getFI(te, h);
	}
	
	rpmteSetHeader(te, h);
	headerFree(h);
    }

exit:
    return (h != NULL);
}

static int rpmteClose(rpmte te, int reset_fi)
{
    if (te == NULL || te->ts == NULL)
	return 0;

    switch (te->type) {
    case TR_ADDED:
	if (te->fd) {
	    rpmtsNotify(te->ts, te, RPMCALLBACK_INST_CLOSE_FILE, 0, 0);
	    te->fd = NULL;
	}
	break;
    case TR_REMOVED:
	/* eventually we'll want notifications for erase open too */
	break;
    }
    rpmteSetHeader(te, NULL);
    if (reset_fi) {
	rpmteSetFI(te, NULL);
    }
    return 1;
}

FD_t rpmtePayload(rpmte te)
{
    FD_t payload = NULL;
    if (te->fd && te->h) {
	const char *compr = headerGetString(te->h, RPMTAG_PAYLOADCOMPRESSOR);
	char *ioflags = rstrscat(NULL, "r.", compr ? compr : "gzip", NULL);
	payload = Fdopen(fdDup(Fileno(te->fd)), ioflags);
	free(ioflags);
    }
    return payload;
}

static int rpmteMarkFailed(rpmte te)
{
    rpmtsi pi = rpmtsiInit(te->ts);
    rpmte p;

    te->failed++;
    /* XXX we can do a much better here than this... */
    while ((p = rpmtsiNext(pi, TR_REMOVED))) {
	if (rpmteDependsOn(p) == te) {
	    p->failed++;
	}
    }
    rpmtsiFree(pi);
    return te->failed;
}

int rpmteFailed(rpmte te)
{
    return (te != NULL) ? te->failed : -1;
}

static int rpmteHaveTransScript(rpmte te, rpmTagVal tag)
{
    int rc = 0;
    if (tag == RPMTAG_PRETRANS) {
	rc = (te->transscripts & RPMTE_HAVE_PRETRANS);
    } else if (tag == RPMTAG_POSTTRANS) {
	rc = (te->transscripts & RPMTE_HAVE_POSTTRANS);
    }
    return rc;
}

rpmps rpmteProblems(rpmte te)
{
    return (te != NULL) ? rpmpsLink(te->probs) : NULL;
}

void rpmteCleanProblems(rpmte te)
{
    if (te != NULL && te->probs != NULL) {
	te->probs = rpmpsFree(te->probs);
    }
}

static void appendProblem(rpmte te, rpmProblemType type,
		fnpyKey key, const char * altNEVR,
		const char * str, uint64_t number)
{
    rpmProblem o;
    rpmProblem p = rpmProblemCreate(type, te->NEVRA, key, altNEVR, str, number);
    rpmpsi psi = rpmpsInitIterator(te->probs);

    /* Only add new, unique problems to the set */
    while ((o = rpmpsiNext(psi))) {
	if (rpmProblemCompare(p, o) == 0)
	    break;
    }
    rpmpsFreeIterator(psi);

    if (o == NULL) {
	if (te->probs == NULL)
	    te->probs = rpmpsCreate();
	rpmpsAppendProblem(te->probs, p);
    }
    rpmProblemFree(p);
}

void rpmteAddProblem(rpmte te, rpmProblemType type,
		     const char *altNEVR, const char *str, uint64_t number)
{
    if (te != NULL) {
	appendProblem(te, type, rpmteKey(te), altNEVR, str, number);
    }
}

void rpmteAddDepProblem(rpmte te, const char * altNEVR, rpmds ds,
		        fnpyKey * suggestedKeys)
{
    if (te != NULL) {
	const char * DNEVR = rpmdsDNEVR(ds);
	rpmProblemType type;
	fnpyKey key = (suggestedKeys ? suggestedKeys[0] : NULL);

	switch ((unsigned)DNEVR[0]) {
	case 'O':	type = RPMPROB_OBSOLETES;	break;
	case 'C':	type = RPMPROB_CONFLICT;	break;
	default:
	case 'R':	type = RPMPROB_REQUIRES;	break;
	}

	appendProblem(te, type, key, altNEVR, DNEVR+2, rpmdsInstance(ds));
    }
}

const char * rpmteTypeString(rpmte te)
{
    switch(rpmteType(te)) {
    case TR_ADDED:	return _("install");
    case TR_REMOVED:	return _("erase");
    default:		return "???";
    }
}

rpmfs rpmteGetFileStates(rpmte te) {
    return te->fs;
}

rpmRC rpmteSetupCollectionPlugins(rpmte te)
{
    ARGV_const_t colls = rpmteCollections(te);
    rpmPlugins plugins = rpmtsPlugins(te->ts);
    rpmRC rc = RPMRC_OK;

    if (!colls) {
	return rc;
    }

    rpmteOpen(te, 0);
    for (; colls && *colls; colls++) {
	if (!rpmpluginsPluginAdded(plugins, *colls)) {
	    rc = rpmpluginsAddCollectionPlugin(plugins, *colls);
	    if (rc != RPMRC_OK) {
		break;
	    }
	}
	rc = rpmpluginsCallOpenTE(plugins, *colls, te);
	if (rc != RPMRC_OK) {
	    break;
	}
    }
    rpmteClose(te, 0);

    return rc;
}

static rpmRC rpmteRunAllCollections(rpmte te, rpmPluginHook hook)
{
    ARGV_const_t colls;
    rpmRC(*collHook) (rpmPlugins, const char *);
    rpmRC rc = RPMRC_OK;

    if (rpmtsFlags(te->ts) & RPMTRANS_FLAG_NOCOLLECTIONS) {
	goto exit;
    }

    switch (hook) {
    case PLUGINHOOK_COLL_POST_ADD:
	colls = te->lastInCollectionsAdd;
	collHook = rpmpluginsCallCollectionPostAdd;
	break;
    case PLUGINHOOK_COLL_POST_ANY:
	colls = te->lastInCollectionsAny;
	collHook = rpmpluginsCallCollectionPostAny;
	break;
    case PLUGINHOOK_COLL_PRE_REMOVE:
	colls = te->firstInCollectionsRemove;
	collHook = rpmpluginsCallCollectionPreRemove;
	break;
    default:
	goto exit;
    }

    for (; colls && *colls; colls++) {
	rc = collHook(rpmtsPlugins(te->ts), *colls);
    }

  exit:
    return rc;
}

int rpmteProcess(rpmte te, pkgGoal goal)
{
    /* Only install/erase resets pkg file info */
    int scriptstage = (goal != PKG_INSTALL && goal != PKG_ERASE);
    int reset_fi = (scriptstage == 0);
    int failed = 1;

    /* Dont bother opening for elements without pre/posttrans scripts */
    if (goal == PKG_PRETRANS || goal == PKG_POSTTRANS) {
	if (!rpmteHaveTransScript(te, goal)) {
	    return 0;
	}
    }

    if (!scriptstage) {
	rpmteRunAllCollections(te, PLUGINHOOK_COLL_PRE_REMOVE);
    }

    if (rpmteOpen(te, reset_fi)) {
	failed = rpmpsmRun(te->ts, te, goal);
	rpmteClose(te, reset_fi);
    }
    
    if (!scriptstage) {
	rpmteRunAllCollections(te, PLUGINHOOK_COLL_POST_ADD);
	rpmteRunAllCollections(te, PLUGINHOOK_COLL_POST_ANY);
    }

    /* XXX should %pretrans failure fail the package install? */
    if (failed && !scriptstage) {
	failed = rpmteMarkFailed(te);
    }

    return failed;
}
