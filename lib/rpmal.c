/** \ingroup rpmdep
 * \file lib/rpmal.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rpmal.h"
#include "rpmds.h"
#include "rpmfi.h"

#include "debug.h"

typedef /*@abstract@*/ struct availablePackage_s * availablePackage;

/*@unchecked@*/
int _rpmal_debug = 0;

/*@access alKey @*/
/*@access alNum @*/
/*@access rpmal @*/
/*@access availablePackage @*/

/*@access fnpyKey @*/	/* XXX suggestedKeys array */

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
/*@refcounted@*/ /*@null@*/
    rpmds provides;		/*!< Provides: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmfi fi;			/*!< File info set. */

    uint_32 tscolor;		/*!< Transaction color bits. */

/*@exposed@*/ /*@dependent@*/ /*@null@*/
    fnpyKey key;		/*!< Associated file name/python object */

};

typedef /*@abstract@*/ struct availableIndexEntry_s *	availableIndexEntry;
/*@access availableIndexEntry@*/

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
struct availableIndexEntry_s {
/*@exposed@*/ /*@dependent@*/ /*@null@*/
    alKey pkgKey;		/*!< Containing package. */
/*@observer@*/
    const char * entry;		/*!< Dependency name. */
    unsigned short entryLen;	/*!< No. of bytes in name. */
    unsigned short entryIx;	/*!< Dependency index. */
    enum indexEntryType {
	IET_PROVIDES=1			/*!< A Provides: dependency. */
    } type;			/*!< Type of available item. */
};

typedef /*@abstract@*/ struct availableIndex_s *	availableIndex;
/*@access availableIndex@*/

/** \ingroup rpmdep
 * Index of all available items.
 */
struct availableIndex_s {
/*@null@*/
    availableIndexEntry index;	/*!< Array of available items. */
    int size;			/*!< No. of available items. */
    int k;			/*!< Current index. */
};

typedef /*@abstract@*/ struct fileIndexEntry_s *	fileIndexEntry;
/*@access fileIndexEntry@*/

/** \ingroup rpmdep
 * A file to be installed/removed.
 */
struct fileIndexEntry_s {
/*@dependent@*/ /*@relnull@*/
    const char * baseName;	/*!< File basename. */
    int baseNameLen;
    alNum pkgNum;		/*!< Containing package index. */
    uint_32 ficolor;
};

typedef /*@abstract@*/ struct dirInfo_s *		dirInfo;
/*@access dirInfo@*/

/** \ingroup rpmdep
 * A directory to be installed/removed.
 */
struct dirInfo_s {
/*@owned@*/ /*@relnull@*/
    const char * dirName;	/*!< Directory path (+ trailing '/'). */
    int dirNameLen;		/*!< No. bytes in directory path. */
/*@owned@*/
    fileIndexEntry files;	/*!< Array of files in directory. */
    int numFiles;		/*!< No. files in directory. */
};

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
struct rpmal_s {
/*@owned@*/ /*@null@*/
    availablePackage list;	/*!< Set of packages. */
    struct availableIndex_s index;	/*!< Set of available items. */
    int delta;			/*!< Delta for pkg list reallocation. */
    int size;			/*!< No. of pkgs in list. */
    int alloced;		/*!< No. of pkgs allocated for list. */
    uint_32 tscolor;		/*!< Transaction color. */
    int numDirs;		/*!< No. of directories. */
/*@owned@*/ /*@null@*/
    dirInfo dirs;		/*!< Set of directories. */
};

/**
 * Destroy available item index.
 * @param al		available list
 */
static void rpmalFreeIndex(rpmal al)
	/*@modifies al @*/
{
    availableIndex ai = &al->index;
    if (ai->size > 0) {
	ai->index = _free(ai->index);
	ai->size = 0;
    }
}

#ifdef	DYING
/**
 * Return number of packages in list.
 * @param al		available list
 * @return		no. of packages in list
 */
static int alGetSize(/*@null@*/ const rpmal al)
	/*@*/
{
    return (al != NULL ? al->size : 0);
}
#endif

static inline alNum alKey2Num(/*@unused@*/ /*@null@*/ const rpmal al,
		/*@null@*/ alKey pkgKey)
	/*@*/
{
    /*@-nullret -temptrans -retalias @*/
    return ((alNum)pkgKey);
    /*@=nullret =temptrans =retalias @*/
}

static inline alKey alNum2Key(/*@unused@*/ /*@null@*/ const rpmal al,
		/*@null@*/ alNum pkgNum)
	/*@*/
{
    /*@-nullret -temptrans -retalias @*/
    return ((alKey)pkgNum);
    /*@=nullret =temptrans =retalias @*/
}

#ifdef	DYING
/**
 * Return available package.
 * @param al		available list
 * @param pkgKey	available package key
 * @return		available package pointer
 */
/*@dependent@*/ /*@null@*/
static availablePackage alGetPkg(/*@null@*/ const rpmal al,
		/*@null@*/ alKey pkgKey)
	/*@*/
{
    alNum pkgNum = alKey2Num(al, pkgKey);
    availablePackage alp = NULL;

    if (al != NULL && pkgNum >= 0 && pkgNum < alGetSize(al)) {
	if (al->list != NULL)
	    alp = al->list + pkgNum;
    }
    return alp;
}
#endif

rpmal rpmalCreate(int delta)
{
    rpmal al = xcalloc(1, sizeof(*al));
    availableIndex ai = &al->index;

    al->delta = delta;
    al->size = 0;
    al->list = xcalloc(al->delta, sizeof(*al->list));
    al->alloced = al->delta;

    ai->index = NULL;
    ai->size = 0;

    al->numDirs = 0;
    al->dirs = NULL;
    return al;
}

rpmal rpmalFree(rpmal al)
{
    availablePackage alp;
    dirInfo die;
    int i;

    if (al == NULL)
	return NULL;

    if ((alp = al->list) != NULL)
    for (i = 0; i < al->size; i++, alp++) {
	alp->provides = rpmdsFree(alp->provides);
	alp->fi = rpmfiFree(alp->fi);
    }

    if ((die = al->dirs) != NULL)
    for (i = 0; i < al->numDirs; i++, die++) {
	die->dirName = _free(die->dirName);
	die->files = _free(die->files);
    }
    al->dirs = _free(al->dirs);
    al->numDirs = 0;

    al->list = _free(al->list);
    al->alloced = 0;
    rpmalFreeIndex(al);
    al = _free(al);
    return NULL;
}

/**
 * Compare two directory info entries by name (qsort/bsearch).
 * @param one		1st directory info
 * @param two		2nd directory info
 * @return		result of comparison
 */
static int dieCompare(const void * one, const void * two)
	/*@*/
{
    /*@-castexpose@*/
    const dirInfo a = (const dirInfo) one;
    const dirInfo b = (const dirInfo) two;
    /*@=castexpose@*/
    int lenchk = a->dirNameLen - b->dirNameLen;

    if (lenchk || a->dirNameLen == 0)
	return lenchk;

    if (a->dirName == NULL || b->dirName == NULL)
	return lenchk;

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->dirName, b->dirName);
}

/**
 * Compare two file info entries by name (qsort/bsearch).
 * @param one		1st directory info
 * @param two		2nd directory info
 * @return		result of comparison
 */
static int fieCompare(const void * one, const void * two)
	/*@*/
{
    /*@-castexpose@*/
    const fileIndexEntry a = (const fileIndexEntry) one;
    const fileIndexEntry b = (const fileIndexEntry) two;
    /*@=castexpose@*/
    int lenchk = a->baseNameLen - b->baseNameLen;

    if (lenchk)
	return lenchk;

    if (a->baseName == NULL || b->baseName == NULL)
	return lenchk;

#ifdef	NOISY
/*@-modfilesys@*/
if (_rpmal_debug) {
fprintf(stderr, "\t\tstrcmp(%p:%p, %p:%p)", a, a->baseName, b, b->baseName);
#if 0
fprintf(stderr, " a %s", a->baseName);
#endif
fprintf(stderr, " b %s", a->baseName);
fprintf(stderr, "\n");
}
/*@=modfilesys@*/
#endif

    return strcmp(a->baseName, b->baseName);
}

void rpmalDel(rpmal al, alKey pkgKey)
{
    alNum pkgNum = alKey2Num(al, pkgKey);
    availablePackage alp;
    rpmfi fi;

    if (al == NULL || al->list == NULL)
	return;		/* XXX can't happen */

    alp = al->list + pkgNum;

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "*** del %p[%d]\n", al->list, pkgNum);
/*@=modfilesys@*/

    /* Delete directory/file info entries from added package list. */
    if ((fi = alp->fi) != NULL)
    if (rpmfiFC(fi) > 0) {
	int origNumDirs = al->numDirs;
	int dx;
	dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
	dirInfo die;
	int last;
	int i;

	/* XXX FIXME: We ought to relocate the directory list here */

	if (al->dirs != NULL)
	for (dx = rpmfiDC(fi) - 1; dx >= 0; dx--)
	{
	    fileIndexEntry fie;

	    (void) rpmfiSetDX(fi, dx);

	    /*@-assignexpose -dependenttrans -observertrans@*/
	    dieNeedle->dirName = (char *) rpmfiDN(fi);
	    /*@=assignexpose =dependenttrans =observertrans@*/
	    dieNeedle->dirNameLen = (dieNeedle->dirName != NULL
			? strlen(dieNeedle->dirName) : 0);
/*@-boundswrite@*/
	    die = bsearch(dieNeedle, al->dirs, al->numDirs,
			       sizeof(*dieNeedle), dieCompare);
/*@=boundswrite@*/
	    if (die == NULL)
		continue;

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "--- die[%5ld] %p [%3d] %s\n", (die - al->dirs), die, die->dirNameLen, die->dirName);
/*@=modfilesys@*/

	    last = die->numFiles;
	    fie = die->files + last - 1;
	    for (i = last - 1; i >= 0; i--, fie--) {
		if (fie->pkgNum != pkgNum)
		    /*@innercontinue@*/ continue;
		die->numFiles--;

		if (i < die->numFiles) {
/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "\t%p[%3d] memmove(%p:%p,%p:%p,0x%lx) %s <- %s\n", die->files, die->numFiles, fie, fie->baseName, fie+1, (fie+1)->baseName, ((die->numFiles - i) * sizeof(*fie)), fie->baseName, (fie+1)->baseName);
/*@=modfilesys@*/

/*@-bounds@*/
		    memmove(fie, fie+1, (die->numFiles - i) * sizeof(*fie));
/*@=bounds@*/
		}
/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "\t%p[%3d] memset(%p,0,0x%lx) %p [%3d] %s\n", die->files, die->numFiles, die->files + die->numFiles, sizeof(*fie), fie->baseName, fie->baseNameLen, fie->baseName);
/*@=modfilesys@*/
		memset(die->files + die->numFiles, 0, sizeof(*fie)); /* overkill */

	    }
	    if (die->numFiles > 0) {
		if (last > i)
		    die->files = xrealloc(die->files,
					die->numFiles * sizeof(*die->files));
		continue;
	    }
	    die->files = _free(die->files);
	    die->dirName = _free(die->dirName);
	    al->numDirs--;
	    if ((die - al->dirs) < al->numDirs) {
/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "    die[%5ld] memmove(%p,%p,0x%lx)\n", (die - al->dirs), die, die+1, ((al->numDirs - (die - al->dirs)) * sizeof(*die)));
/*@=modfilesys@*/

/*@-bounds@*/
		memmove(die, die+1, (al->numDirs - (die - al->dirs)) * sizeof(*die));
/*@=bounds@*/
	    }

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "    die[%5d] memset(%p,0,0x%lx)\n", al->numDirs, al->dirs + al->numDirs, sizeof(*die));
/*@=modfilesys@*/
	    memset(al->dirs + al->numDirs, 0, sizeof(*al->dirs)); /* overkill */
	}

	if (origNumDirs > al->numDirs) {
	    if (al->numDirs > 0)
		al->dirs = xrealloc(al->dirs, al->numDirs * sizeof(*al->dirs));
	    else
		al->dirs = _free(al->dirs);
	}
    }

    alp->provides = rpmdsFree(alp->provides);
    alp->fi = rpmfiFree(alp->fi);

/*@-boundswrite@*/
    memset(alp, 0, sizeof(*alp));	/* XXX trash and burn */
/*@=boundswrite@*/
    return;
}

/*@-bounds@*/
alKey rpmalAdd(rpmal * alistp, alKey pkgKey, fnpyKey key,
		rpmds provides, rpmfi fi, uint_32 tscolor)
{
    alNum pkgNum;
    rpmal al;
    availablePackage alp;

    /* If list doesn't exist yet, create. */
    if (*alistp == NULL)
	*alistp = rpmalCreate(5);
    al = *alistp;
    pkgNum = alKey2Num(al, pkgKey);

    if (pkgNum >= 0 && pkgNum < al->size) {
	rpmalDel(al, pkgKey);
    } else {
	if (al->size == al->alloced) {
	    al->alloced += al->delta;
	    al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
	}
	pkgNum = al->size++;
    }

    if (al->list == NULL)
	return RPMAL_NOMATCH;		/* XXX can't happen */

    alp = al->list + pkgNum;

    alp->key = key;
    alp->tscolor = tscolor;

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "*** add %p[%d] 0x%x\n", al->list, pkgNum, tscolor);
/*@=modfilesys@*/

    alp->provides = rpmdsLink(provides, "Provides (rpmalAdd)");
    alp->fi = rpmfiLink(fi, "Files (rpmalAdd)");

    fi = rpmfiLink(alp->fi, "Files index (rpmalAdd)");
    fi = rpmfiInit(fi, 0);
    if (rpmfiFC(fi) > 0) {
	dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
	dirInfo die;
	int dc = rpmfiDC(fi);
	int dx;
	int * dirMapping = alloca(sizeof(*dirMapping) * dc);
	int * dirUnique = alloca(sizeof(*dirUnique) * dc);
	const char * DN;
	int origNumDirs;
	int first;
	int i;

	/* XXX FIXME: We ought to relocate the directory list here */

	/* XXX enough space for all directories, late realloc to truncate. */
	al->dirs = xrealloc(al->dirs, (al->numDirs + dc) * sizeof(*al->dirs));

	/* Only previously allocated dirInfo is sorted and bsearch'able. */
	origNumDirs = al->numDirs;

	/* Package dirnames are not currently unique. Create unique mapping. */
	for (dx = 0; dx < dc; dx++) {
	    (void) rpmfiSetDX(fi, dx);
	    DN = rpmfiDN(fi);
	    if (DN != NULL)
	    for (i = 0; i < dx; i++) {
		const char * iDN;
		(void) rpmfiSetDX(fi, i);
		iDN = rpmfiDN(fi);
		if (iDN != NULL && !strcmp(DN, iDN))
		    /*@innerbreak@*/ break;
	    }
	    dirUnique[dx] = i;
	}

	/* Map package dirs into transaction dirInfo index. */
	for (dx = 0; dx < dc; dx++) {

	    /* Non-unique package dirs use the 1st entry mapping. */
	    if (dirUnique[dx] < dx) {
		dirMapping[dx] = dirMapping[dirUnique[dx]];
		continue;
	    }

	    /* Find global dirInfo mapping for first encounter. */
	    (void) rpmfiSetDX(fi, dx);

	    /*@-assignexpose -dependenttrans -observertrans@*/
	    {   DN = rpmfiDN(fi);

#if defined(__ia64__)
/* XXX Make sure that autorelocated file dependencies are satisfied. */
#define	DNPREFIX	"/emul/ia32-linux"
		if (!strncmp(DN, DNPREFIX, sizeof(DNPREFIX)-1))
		    DN += sizeof(DNPREFIX)-1;
#endif
		dieNeedle->dirName = DN;
	    }
	    /*@=assignexpose =dependenttrans =observertrans@*/

	    dieNeedle->dirNameLen = (dieNeedle->dirName != NULL
			? strlen(dieNeedle->dirName) : 0);
	    die = bsearch(dieNeedle, al->dirs, origNumDirs,
			       sizeof(*dieNeedle), dieCompare);
	    if (die) {
		dirMapping[dx] = die - al->dirs;
	    } else {
		dirMapping[dx] = al->numDirs;
		die = al->dirs + al->numDirs;
		if (dieNeedle->dirName != NULL)
		    die->dirName = xstrdup(dieNeedle->dirName);
		die->dirNameLen = dieNeedle->dirNameLen;
		die->files = NULL;
		die->numFiles = 0;
/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "+++ die[%5d] %p [%3d] %s\n", al->numDirs, die, die->dirNameLen, die->dirName);
/*@=modfilesys@*/

		al->numDirs++;
	    }
	}

	for (first = rpmfiNext(fi); first >= 0;) {
	    fileIndexEntry fie;
	    int next;

	    /* Find the first file of the next directory. */
	    dx = rpmfiDX(fi);
	    while ((next = rpmfiNext(fi)) >= 0) {
		if (dx != rpmfiDX(fi))
		    /*@innerbreak@*/ break;
	    }
	    if (next < 0) next = rpmfiFC(fi);	/* XXX reset end-of-list */

	    die = al->dirs + dirMapping[dx];
	    die->files = xrealloc(die->files,
			(die->numFiles + next - first) * sizeof(*die->files));

	    fie = die->files + die->numFiles;

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "    die[%5d] %p->files [%p[%d],%p) -> [%p[%d],%p)\n", dirMapping[dx], die,
die->files, die->numFiles, die->files+die->numFiles,
fie, (next - first), fie + (next - first));
/*@=modfilesys@*/

	    /* Rewind to first file, generate file index entry for each file. */
	    fi = rpmfiInit(fi, first);
	    while ((first = rpmfiNext(fi)) >= 0 && first < next) {
		/*@-assignexpose -dependenttrans -observertrans @*/
		fie->baseName = rpmfiBN(fi);
		/*@=assignexpose =dependenttrans =observertrans @*/
		fie->baseNameLen = (fie->baseName ? strlen(fie->baseName) : 0);
		fie->pkgNum = pkgNum;
		fie->ficolor = rpmfiFColor(fi);
/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "\t%p[%3d] %p:%p[%2d] %s\n", die->files, die->numFiles, fie, fie->baseName, fie->baseNameLen, rpmfiFN(fi));
/*@=modfilesys@*/

		die->numFiles++;
		fie++;
	    }
	    qsort(die->files, die->numFiles, sizeof(*die->files), fieCompare);
	}

	/* Resize the directory list. If any directories were added, resort. */
	al->dirs = xrealloc(al->dirs, al->numDirs * sizeof(*al->dirs));
	if (origNumDirs != al->numDirs)
	    qsort(al->dirs, al->numDirs, sizeof(*al->dirs), dieCompare);
    }
    fi = rpmfiUnlink(fi, "Files index (rpmalAdd)");

    rpmalFreeIndex(al);

assert(((alNum)(alp - al->list)) == pkgNum);
    return ((alKey)(alp - al->list));
}
/*@=bounds@*/

/**
 * Compare two available index entries by name (qsort/bsearch).
 * @param one		1st available index entry
 * @param two		2nd available index entry
 * @return		result of comparison
 */
static int indexcmp(const void * one, const void * two)
	/*@*/
{
    /*@-castexpose@*/
    const availableIndexEntry a = (const availableIndexEntry) one;
    const availableIndexEntry b = (const availableIndexEntry) two;
    /*@=castexpose@*/
    int lenchk;

    lenchk = a->entryLen - b->entryLen;
    if (lenchk)
	return lenchk;

    return strcmp(a->entry, b->entry);
}

void rpmalAddProvides(rpmal al, alKey pkgKey, rpmds provides, uint_32 tscolor)
{
    uint_32 dscolor;
    const char * Name;
    alNum pkgNum = alKey2Num(al, pkgKey);
    availableIndex ai = &al->index;
    availableIndexEntry aie;
    int ix;

    if (provides == NULL || pkgNum < 0 || pkgNum >= al->size)
	return;
    if (ai->index == NULL || ai->k < 0 || ai->k >= ai->size)
	return;

    if (rpmdsInit(provides) != NULL)
    while (rpmdsNext(provides) >= 0) {

	if ((Name = rpmdsN(provides)) == NULL)
	    continue;	/* XXX can't happen */

	/* Ignore colored provides not in our rainbow. */
	dscolor = rpmdsColor(provides);
	if (tscolor && dscolor && !(tscolor & dscolor))
	    continue;

	aie = ai->index + ai->k;
	ai->k++;

	aie->pkgKey = pkgKey;
	aie->entry = Name;
	aie->entryLen = strlen(Name);
	ix = rpmdsIx(provides);

/* XXX make sure that element index fits in unsigned short */
assert(ix < 0x10000);

	aie->entryIx = ix;
	aie->type = IET_PROVIDES;
    }
}

void rpmalMakeIndex(rpmal al)
{
    availableIndex ai;
    availablePackage alp;
    int i;

    if (al == NULL || al->list == NULL) return;
    ai = &al->index;

    ai->size = 0;
    for (i = 0; i < al->size; i++) {
	alp = al->list + i;
	if (alp->provides != NULL)
	    ai->size += rpmdsCount(alp->provides);
    }
    if (ai->size == 0) return;

    ai->index = xrealloc(ai->index, ai->size * sizeof(*ai->index));
    ai->k = 0;
    for (i = 0; i < al->size; i++) {
	alp = al->list + i;
	rpmalAddProvides(al, (alKey)i, alp->provides, alp->tscolor);
    }

    /* Reset size to the no. of provides added. */
    ai->size = ai->k;
    qsort(ai->index, ai->size, sizeof(*ai->index), indexcmp);
}

fnpyKey *
rpmalAllFileSatisfiesDepend(const rpmal al, const rpmds ds, alKey * keyp)
{
    uint_32 tscolor;
    uint_32 ficolor;
    int found = 0;
    const char * dirName;
    const char * baseName;
    dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
    dirInfo die;
    fileIndexEntry fieNeedle =
		memset(alloca(sizeof(*fieNeedle)), 0, sizeof(*fieNeedle));
    fileIndexEntry fie;
    availablePackage alp;
    fnpyKey * ret = NULL;
    const char * fileName;

    if (keyp) *keyp = RPMAL_NOMATCH;

    if (al == NULL || (fileName = rpmdsN(ds)) == NULL || *fileName != '/')
	return NULL;

    /* Solaris 2.6 bsearch sucks down on this. */
    if (al->numDirs == 0 || al->dirs == NULL || al->list == NULL)
	return NULL;

    {	char * t;
	dirName = t = xstrdup(fileName);
	if ((t = strrchr(t, '/')) != NULL) {
	    t++;		/* leave the trailing '/' */
	    *t = '\0';
	}
    }

    dieNeedle->dirName = (char *) dirName;
    dieNeedle->dirNameLen = strlen(dirName);
    die = bsearch(dieNeedle, al->dirs, al->numDirs,
		       sizeof(*dieNeedle), dieCompare);
    if (die == NULL)
	goto exit;

    /* rewind to the first match */
    while (die > al->dirs && dieCompare(die-1, dieNeedle) == 0)
	die--;

    if ((baseName = strrchr(fileName, '/')) == NULL)
	goto exit;
    baseName++;

    /*@-branchstate@*/ /* FIX: ret is a problem */
    for (found = 0, ret = NULL;
	 die < al->dirs + al->numDirs && dieCompare(die, dieNeedle) == 0;
	 die++)
    {

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "==> die %p %s\n", die, (die->dirName ? die->dirName : "(nil)"));
/*@=modfilesys@*/

/*@-observertrans@*/
	fieNeedle->baseName = baseName;
/*@=observertrans@*/
	fieNeedle->baseNameLen = strlen(fieNeedle->baseName);
	fie = bsearch(fieNeedle, die->files, die->numFiles,
		       sizeof(*fieNeedle), fieCompare);
	if (fie == NULL)
	    continue;	/* XXX shouldn't happen */

/*@-modfilesys@*/
if (_rpmal_debug)
fprintf(stderr, "==> fie %p %s\n", fie, (fie->baseName ? fie->baseName : "(nil)"));
/*@=modfilesys@*/

	alp = al->list + fie->pkgNum;

        /* Ignore colored files not in our rainbow. */
	tscolor = alp->tscolor;
	ficolor = fie->ficolor;
        if (tscolor && ficolor && !(tscolor & ficolor))
            continue;

	rpmdsNotify(ds, _("(added files)"), 0);

	ret = xrealloc(ret, (found+2) * sizeof(*ret));
	if (ret)	/* can't happen */
	    ret[found] = alp->key;
	if (keyp)
	    *keyp = alNum2Key(al, fie->pkgNum);
	found++;
    }
    /*@=branchstate@*/

exit:
    dirName = _free(dirName);
    if (ret)
	ret[found] = NULL;
    return ret;
}

fnpyKey *
rpmalAllSatisfiesDepend(const rpmal al, const rpmds ds, alKey * keyp)
{
    availableIndex ai;
    availableIndexEntry needle;
    availableIndexEntry match;
    fnpyKey * ret = NULL;
    int found = 0;
    const char * KName;
    availablePackage alp;
    int rc;

    if (keyp) *keyp = RPMAL_NOMATCH;

    if (al == NULL || ds == NULL || (KName = rpmdsN(ds)) == NULL)
	return ret;

    if (*KName == '/') {
	/* First, look for files "contained" in package ... */
	ret = rpmalAllFileSatisfiesDepend(al, ds, keyp);
	if (ret != NULL && *ret != NULL)
	    return ret;
	/* ... then, look for files "provided" by package. */
	ret = _free(ret);
    }

    ai = &al->index;
    if (ai->index == NULL || ai->size <= 0)
	return NULL;

    needle = memset(alloca(sizeof(*needle)), 0, sizeof(*needle));
    /*@-assignexpose -temptrans@*/
    needle->entry = KName;
    /*@=assignexpose =temptrans@*/
    needle->entryLen = strlen(needle->entry);

    match = bsearch(needle, ai->index, ai->size, sizeof(*ai->index), indexcmp);
    if (match == NULL)
	return NULL;

    /* rewind to the first match */
    while (match > ai->index && indexcmp(match-1, needle) == 0)
	match--;

    if (al->list != NULL)	/* XXX always true */
    for (ret = NULL, found = 0;
	 match < ai->index + ai->size && indexcmp(match, needle) == 0;
	 match++)
    {
	alp = al->list + alKey2Num(al, match->pkgKey);

	rc = 0;
	if (alp->provides != NULL)	/* XXX can't happen */
	switch (match->type) {
	case IET_PROVIDES:
	    /* XXX single step on rpmdsNext to regenerate DNEVR string */
	    (void) rpmdsSetIx(alp->provides, match->entryIx - 1);
	    if (rpmdsNext(alp->provides) >= 0)
		rc = rpmdsCompare(alp->provides, ds);

	    if (rc)
		rpmdsNotify(ds, _("(added provide)"), 0);

	    /*@switchbreak@*/ break;
	}

	/*@-branchstate@*/
	if (rc) {
	    ret = xrealloc(ret, (found + 2) * sizeof(*ret));
	    if (ret)	/* can't happen */
		ret[found] = alp->key;
/*@-dependenttrans@*/
	    if (keyp)
		*keyp = match->pkgKey;
/*@=dependenttrans@*/
	    found++;
	}
	/*@=branchstate@*/
    }

    if (ret)
	ret[found] = NULL;

/*@-nullstate@*/ /* FIX: *keyp may be NULL */
    return ret;
/*@=nullstate@*/
}

fnpyKey
rpmalSatisfiesDepend(const rpmal al, const rpmds ds, alKey * keyp)
{
    fnpyKey * tmp = rpmalAllSatisfiesDepend(al, ds, keyp);

    if (tmp) {
	fnpyKey ret = tmp[0];
	free(tmp);
	return ret;
    }
    return NULL;
}
