/** \ingroup rpmdep
 * \file lib/rpmal.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rpmds.h"
#include "depends.h"
#include "rpmal.h"

#include "debug.h"

/*@access availableList@*/

#ifdef	DYING
/*@access tsortInfo@*/
#endif

/*@access alKey@*/
/*@access alNum@*/
/*@access fnpyKey@*/

/*@access rpmFNSet@*/

typedef /*@abstract@*/ struct availablePackage_s * availablePackage;
/*@access availablePackage@*/

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
/*@refcounted@*/ /*@null@*/
    rpmDepSet provides;		/*!< Provides: dependencies. */
/*@refcounted@*/ /*@null@*/
    rpmFNSet fns;		/*!< File info set. */

#ifdef	DYING
    uint_32 multiLib;	/* MULTILIB */
#endif

    fnpyKey key;		/*!< Associated file name/python object */

};

typedef /*@abstract@*/ struct availableIndexEntry_s *	availableIndexEntry;
/*@access availableIndexEntry@*/

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
struct availableIndexEntry_s {
/*@dependent@*/ /*@null@*/
    alKey pkgKey;		/*!< Containing package. */
/*@dependent@*/
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
/*@dependent@*/
    const char * baseName;	/*!< File basename. */
    int baseNameLen;
    alNum pkgNum;		/*!< Containing package index. */
    int fileFlags;	/* MULTILIB */
};

typedef /*@abstract@*/ struct dirInfo_s *		dirInfo;
/*@access dirInfo@*/

/** \ingroup rpmdep
 * A directory to be installed/removed.
 */
struct dirInfo_s {
/*@owned@*/
    const char * dirName;	/*!< Directory path (+ trailing '/'). */
    int dirNameLen;		/*!< No. bytes in directory path. */
/*@owned@*/
    fileIndexEntry files;	/*!< Array of files in directory. */
    int numFiles;		/*!< No. files in directory. */
};

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
struct availableList_s {
/*@owned@*/ /*@null@*/
    availablePackage list;	/*!< Set of packages. */
    struct availableIndex_s index;	/*!< Set of available items. */
    int delta;			/*!< Delta for pkg list reallocation. */
    int size;			/*!< No. of pkgs in list. */
    int alloced;		/*!< No. of pkgs allocated for list. */
    int numDirs;		/*!< No. of directories. */
/*@owned@*/ /*@null@*/
    dirInfo dirs;		/*!< Set of directories. */
};

/*@unchecked@*/
static int _al_debug = 0;

/**
 * Destroy available item index.
 * @param al		available list
 */
static void alFreeIndex(availableList al)
	/*@modifies al @*/
{
    availableIndex ai = &al->index;
    if (ai->size > 0) {
	ai->index = _free(ai->index);
	ai->size = 0;
    }
}

/**
 * Return number of packages in list.
 * @param al		available list
 * @return		no. of packages in list
 */
static int alGetSize(/*@null@*/ const availableList al)
	/*@*/
{
    return (al != NULL ? al->size : 0);
}

static inline alNum alKey2Num(/*@unused@*/ /*@null@*/ const availableList al,
		/*@null@*/ alKey pkgKey)
	/*@*/
{
    /*@-nullret -temptrans -retalias @*/
    return ((alNum)pkgKey);
    /*@=nullret =temptrans =retalias @*/
}

static inline alKey alNum2Key(/*@unused@*/ /*@null@*/ const availableList al,
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
static availablePackage alGetPkg(/*@null@*/ const availableList al,
		/*@null@*/ alKey pkgKey)
	/*@*/
{
    availablePackage alp = NULL;
    alNum pkgNum = alKey2Num(al, pkgKey);

    if (al != NULL && pkgNum >= 0 && pkgNum < alGetSize(al)) {
	if (al->list != NULL)
	    alp = al->list + pkgNum;
    }
/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** alp[%d] %p\n", pkgNum, alp);
/*@=modfilesys@*/
    return alp;
}
#endif

availableList alCreate(int delta)
{
    availableList al = xcalloc(1, sizeof(*al));
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

availableList alFree(availableList al)
{
    availablePackage alp;
    dirInfo die;
    int i;

    if (al == NULL)
	return NULL;

    if ((alp = al->list) != NULL)
    for (i = 0; i < al->size; i++, alp++) {
	alp->provides = dsFree(alp->provides);
	alp->fns = fnsFree(alp->fns);
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
    alFreeIndex(al);
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

    if (lenchk)
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

    /* XXX FIXME: this might do "backward" strcmp for speed */
    return strcmp(a->baseName, b->baseName);
}

void alDelPackage(availableList al, alKey pkgKey)
{
    availablePackage alp;
    rpmFNSet fns;
    alNum pkgNum = alKey2Num(al, pkgKey);

    /*@-nullptrarith@*/ /* FIX: al->list might be NULL */
    alp = al->list + pkgNum;
    /*@=nullptrarith@*/

/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** del %p[%d]\n", al->list, pkgNum);
/*@=modfilesys@*/

    /* Delete directory/file info entries from added package list. */
    if ((fns = alp->fns) != NULL)
    if (fns->bnl != NULL && fns->fc > 0) {
	int origNumDirs = al->numDirs;
	int dirNum;
	dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
	dirInfo die;
	int last;
	int i;

	/* XXX FIXME: We ought to relocate the directory list here */

	if (al->dirs != NULL)
	if (fns->dnl != NULL)
	for (dirNum = fns->dc - 1; dirNum >= 0; dirNum--) {
	    fileIndexEntry fie;

	    /*@-assignexpose@*/
	    dieNeedle->dirName = (char *) fns->dnl[dirNum];
	    /*@=assignexpose@*/
	    dieNeedle->dirNameLen = strlen(dieNeedle->dirName);
	    die = bsearch(dieNeedle, al->dirs, al->numDirs,
			       sizeof(*dieNeedle), dieCompare);
	    if (die == NULL)
		continue;

	    last = die->numFiles;
	    fie = die->files + last - 1;
	    for (i = last - 1; i >= 0; i--, fie--) {
		if (fie->pkgNum != pkgNum)
		    /*@innercontinue@*/ continue;
		die->numFiles--;
		if (i > die->numFiles)
		    /*@innercontinue@*/ continue;
		memmove(fie, fie+1, (die->numFiles - i));
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
	    if ((die - al->dirs) > al->numDirs)
		continue;
	    memmove(die, die+1, (al->numDirs - (die - al->dirs)));
	}

	if (origNumDirs > al->numDirs) {
	    if (al->numDirs > 0)
		al->dirs = xrealloc(al->dirs, al->numDirs * sizeof(*al->dirs));
	    else
		al->dirs = _free(al->dirs);
	}
    }

    alp->provides = dsFree(alp->provides);
    alp->fns = fnsFree(alp->fns);

    memset(alp, 0, sizeof(*alp));	/* XXX trash and burn */
    /*@-nullstate@*/ /* FIX: al->list->h may be NULL */
    return;
    /*@=nullstate@*/
}

alKey alAddPackage(availableList al, alKey pkgKey, fnpyKey key,
		rpmDepSet provides, rpmFNSet fns)
{
    availablePackage alp;
    alNum pkgNum = alKey2Num(al, pkgKey);

    if (pkgNum >= 0 && pkgNum < al->size) {
	alDelPackage(al, pkgKey);
    } else {
	if (al->size == al->alloced) {
	    al->alloced += al->delta;
	    al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
	}
	pkgNum = al->size++;
    }

    /*@-nullptrarith@*/
    alp = al->list + pkgNum;
    /*@=nullptrarith@*/

    /*@-assignexpose -temptrans @*/
    alp->key = key;
    /*@=assignexpose =temptrans @*/

/*@-modfilesys@*/
if (_al_debug)
fprintf(stderr, "*** add %p[%d]\n", al->list, pkgNum);
/*@=modfilesys@*/

    /*@-assignexpose -temptrans@*/
    alp->provides = rpmdsLink(provides, "Provides (alAddPackage)");
    alp->fns = rpmfnsLink(fns, "Files (alAddPackage)");
    /*@=assignexpose =temptrans@*/

    if (alp->fns && alp->fns->fc > 0) {
	int * dirMapping;
	dirInfo dieNeedle =
		memset(alloca(sizeof(*dieNeedle)), 0, sizeof(*dieNeedle));
	dirInfo die;
	int first, last, dirNum;
	int origNumDirs;

	/* XXX FIXME: We ought to relocate the directory list here */

	dirMapping = alloca(sizeof(*dirMapping) * alp->fns->dc);

	/* allocated enough space for all the directories we could possible
	   need to add */
	al->dirs = xrealloc(al->dirs,
			(al->numDirs + alp->fns->dc) * sizeof(*al->dirs));
	origNumDirs = al->numDirs;

	if (alp->fns->dnl != NULL)
	for (dirNum = 0; dirNum < alp->fns->dc; dirNum++) {
	    /*@-assignexpose@*/
	    dieNeedle->dirName = (char *) alp->fns->dnl[dirNum];
	    /*@=assignexpose@*/
	    dieNeedle->dirNameLen = strlen(alp->fns->dnl[dirNum]);
	    die = bsearch(dieNeedle, al->dirs, origNumDirs,
			       sizeof(*dieNeedle), dieCompare);
	    if (die) {
		dirMapping[dirNum] = die - al->dirs;
	    } else {
		dirMapping[dirNum] = al->numDirs;
		die = al->dirs + al->numDirs;
		die->dirName = xstrdup(alp->fns->dnl[dirNum]);
		die->dirNameLen = strlen(die->dirName);
		die->files = NULL;
		die->numFiles = 0;
		al->numDirs++;
	    }
	}

	last = 0;
	for (first = 0; first < alp->fns->fc; first = last + 1) {
	    fileIndexEntry fie;

	    if (alp->fns->dil == NULL)	/* XXX can't happen */
		continue;

	    for (last = first; (last + 1) < alp->fns->fc; last++) {
		if (alp->fns->dil[first] != alp->fns->dil[last + 1])
		    /*@innerbreak@*/ break;
	    }

	    die = al->dirs + dirMapping[alp->fns->dil[first]];
	    die->files = xrealloc(die->files,
		    (die->numFiles + last - first + 1) *
			sizeof(*die->files));

	    fie = die->files + die->numFiles;
	    for (alp->fns->i = first; alp->fns->i <= last; alp->fns->i++) {
		if (alp->fns->bnl == NULL)	/* XXX can't happen */
		    /*@innercontinue@*/ continue;
		if (alp->fns->fflags == NULL)	/* XXX can't happen */
		    /*@innercontinue@*/ continue;
		/*@-assignexpose@*/
		fie->baseName = alp->fns->bnl[alp->fns->i];
		fie->baseNameLen = strlen(alp->fns->bnl[alp->fns->i]);
		/*@=assignexpose@*/
		fie->pkgNum = pkgNum;
		fie->fileFlags = alp->fns->fflags[alp->fns->i];
		die->numFiles++;
		fie++;
	    }
	    qsort(die->files, die->numFiles, sizeof(*die->files), fieCompare);
	}

	/* XXX should we realloc al->dirs to actual size? */

	/* If any directories were added, resort the directory list. */
	if (origNumDirs != al->numDirs)
	    qsort(al->dirs, al->numDirs, sizeof(*al->dirs), dieCompare);

    }

    alFreeIndex(al);

assert(((alNum)(alp - al->list)) == pkgNum);
    return ((alKey)(alp - al->list));
}

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

void alAddProvides(availableList al, alKey pkgKey, rpmDepSet provides)
{
    availableIndexEntry aie;
    availableIndex ai = &al->index;
    int i = alKey2Num(al, pkgKey);
    int ix;

    if (provides == NULL || i < 0 || i >= al->size)
	return;
    if (ai->index == NULL || ai->k < 0 || ai->k >= ai->size)
	return;

    if (dsiInit(provides) != NULL)
    while (dsiNext(provides) >= 0) {
	const char * Name;

#ifdef	DYING	/* XXX FIXME: multilib colored dependency search */
	const int_32 Flags = dsiGetFlags(provides);

	/* If multilib install, skip non-multilib provides. */
	if (al->list[i].multiLib && !isDependsMULTILIB(Flags)) {
	    ai->size--;
	    /*@innercontinue@*/ continue;
	}
#endif

	if ((Name = dsiGetN(provides)) == NULL)
	    continue;	/* XXX can't happen */

	aie = ai->index + ai->k;
	ai->k++;

	/*@-assignexpose@*/
	/*@-temptrans@*/
	aie->pkgKey = pkgKey;
	/*@=temptrans@*/
	aie->entry = Name;
	/*@=assignexpose@*/
	aie->entryLen = strlen(Name);
	ix = dsiGetIx(provides);
/* XXX make sure that element index fits in unsigned short */
assert(ix < 0x10000);
	aie->entryIx = ix;
	aie->type = IET_PROVIDES;
    }
}

void alMakeIndex(availableList al)
{
    availableIndex ai = &al->index;
    availablePackage alp;
    int i;

    if (ai->size || al->list == NULL) return;

    for (i = 0; i < al->size; i++) {
	alp = al->list + i;
	if (alp->provides != NULL)
	    ai->size += dsiGetCount(alp->provides);
    }

    if (ai->size) {
	ai->index = xcalloc(ai->size, sizeof(*ai->index));
	ai->k = 0;

	for (i = 0; i < al->size; i++) {
	    alp = al->list + i;
	    alAddProvides(al, (alKey)i, alp->provides);
	}

	qsort(ai->index, ai->size, sizeof(*ai->index), indexcmp);
    }
}

fnpyKey *
alAllFileSatisfiesDepend(const availableList al, const rpmDepSet ds, alKey * keyp)
{
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
    if ((fileName = dsiGetN(ds)) == NULL || *fileName != '/')
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
	 die <= al->dirs + al->numDirs && dieCompare(die, dieNeedle) == 0;
	 die++)
    {

	fieNeedle->baseName = baseName;
	fieNeedle->baseNameLen = strlen(fieNeedle->baseName);
	fie = bsearch(fieNeedle, die->files, die->numFiles,
		       sizeof(*fieNeedle), fieCompare);
	if (fie == NULL)
	    continue;	/* XXX shouldn't happen */

#ifdef	DYING	/* XXX FIXME: multilib colored dependency search */
	/*
	 * If a file dependency would be satisfied by a file
	 * we are not going to install, skip it.
	 */
	if (al->list[fie->pkgNum].multiLib && !isFileMULTILIB(fie->fileFlags))
	    continue;
#endif

	dsiNotify(ds, _("(added files)"), 0);

	alp = al->list + fie->pkgNum;
	ret = xrealloc(ret, (found+2) * sizeof(*ret));
	if (ret)	/* can't happen */
	    ret[found++] = alp->key;
	if (keyp)
	    *keyp = alNum2Key(al, fie->pkgNum);
    }
    /*@=branchstate@*/

exit:
    dirName = _free(dirName);
    /*@-mods@*/		/* AOK: al->list not modified through ret alias. */
    if (ret)
	ret[found] = NULL;
    /*@=mods@*/
    return ret;
}

#ifdef	DYING
/**
 * Check added package file lists for first package that provides a file.
 * @param al		available list
 * @param ds		dependency
 * @return		available package pointer
 */
/*@unused@*/ static /*@dependent@*/ /*@null@*/ fnpyKey
alFileSatisfiesDepend(const availableList al, const rpmDepSet ds, alKey * keyp)
	/*@*/
{
    fnpyKey * tmp = alAllFileSatisfiesDepend(al, ds, keyp);

    if (tmp) {
	fnpyKey ret = tmp[0];
	free(tmp);
	return ret;
    }
    return NULL;
}
#endif	/* DYING */

fnpyKey *
alAllSatisfiesDepend(const availableList al, const rpmDepSet ds, alKey * keyp)
{
    availableIndex ai = &al->index;
    availableIndexEntry needle =
		memset(alloca(sizeof(*needle)), 0, sizeof(*needle));
    availableIndexEntry match;
    fnpyKey * ret = NULL;
    const char * KName;
    availablePackage alp;
    int rc, found;

    if (keyp) *keyp = RPMAL_NOMATCH;
    if ((KName = dsiGetN(ds)) == NULL)
	return ret;

    if (*KName == '/') {
	ret = alAllFileSatisfiesDepend(al, ds, keyp);
	/* XXX Provides: /path was broken with added packages (#52183). */
	if (ret != NULL && *ret != NULL)
	    return ret;
    }

    if (ai->index == NULL || ai->size <= 0)
	return NULL;

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

    for (ret = NULL, found = 0;
	 match <= ai->index + ai->size && indexcmp(match, needle) == 0;
	 match++)
    {
	/*@-nullptrarith@*/
	alp = al->list + alKey2Num(al, match->pkgKey);
	/*@=nullptrarith@*/

	rc = 0;
	if (alp->provides != NULL)	/* XXX can't happen */
	switch (match->type) {
	case IET_PROVIDES:
	    /* XXX single step on dsiNext to regenerate DNEVR string */
	    (void) dsiSetIx(alp->provides, match->entryIx - 1);
	    if (dsiNext(alp->provides) >= 0)
		rc = dsCompare(alp->provides, ds);

	    if (rc)
		dsiNotify(ds, _("(added provide)"), 0);

	    /*@switchbreak@*/ break;
	}

	/*@-branchstate@*/
	if (rc) {
	    ret = xrealloc(ret, (found + 2) * sizeof(*ret));
	    if (ret)	/* can't happen */
		ret[found++] = alp->key;
	    if (keyp)
		*keyp = ((alKey)(alp - al->list));
	}
	/*@=branchstate@*/
    }

    if (ret)
	ret[found] = NULL;

    return ret;
}

fnpyKey
alSatisfiesDepend(const availableList al, const rpmDepSet ds, alKey * keyp)
{
    fnpyKey * tmp = alAllSatisfiesDepend(al, ds, keyp);

    if (tmp) {
	fnpyKey ret = tmp[0];
	free(tmp);
	return ret;
    }
    return NULL;
}
