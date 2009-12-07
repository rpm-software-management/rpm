/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* rpmGetFilesystem*() */
#include <rpm/rpmmacro.h>	/* XXX for %_i18ndomains */
#include <rpm/rpmfi.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>

#include "debug.h"

struct headerTagFunc_s {
    rpmTag tag;		/*!< Tag of extension. */
    void *func;		/*!< Pointer to formatter function. */	
};

/* forward declarations */
static const struct headerTagFunc_s rpmHeaderTagExtensions[];

void *rpmHeaderTagFunc(rpmTag tag);

/** \ingroup rpmfi
 * Retrieve file names from header.
 *
 * The representation of file names in package headers changed in rpm-4.0.
 * Originally, file names were stored as an array of absolute paths.
 * In rpm-4.0, file names are stored as separate arrays of dirname's and
 * basename's, * with a dirname index to associate the correct dirname
 * with each basname.
 *
 * This function is used to retrieve file names independent of how the
 * file names are represented in the package header.
 * 
 * @param h		header
 * @param tagN		RPMTAG_BASENAMES | PMTAG_ORIGBASENAMES
 * @retval *fnp		array of file names
 * @retval *fcp		number of files
 */
static void rpmfiBuildFNames(Header h, rpmTag tagN,
	const char *** fnp, rpm_count_t * fcp)
{
    const char **baseNames, **dirNames, **fileNames;
    uint32_t *dirIndexes;
    rpm_count_t count;
    size_t size;
    rpmTag dirNameTag = 0;
    rpmTag dirIndexesTag = 0;
    char * t;
    int i;
    struct rpmtd_s bnames, dnames, dixs;

    if (tagN == RPMTAG_BASENAMES) {
	dirNameTag = RPMTAG_DIRNAMES;
	dirIndexesTag = RPMTAG_DIRINDEXES;
    } else if (tagN == RPMTAG_ORIGBASENAMES) {
	dirNameTag = RPMTAG_ORIGDIRNAMES;
	dirIndexesTag = RPMTAG_ORIGDIRINDEXES;
    }

    if (!headerGet(h, tagN, &bnames, HEADERGET_MINMEM)) {
	*fnp = NULL;
	*fcp = 0;
	return;		/* no file list */
    }
    (void) headerGet(h, dirNameTag, &dnames, HEADERGET_MINMEM);
    (void) headerGet(h, dirIndexesTag, &dixs, HEADERGET_MINMEM);

    count = rpmtdCount(&bnames);
    baseNames = bnames.data;
    dirNames = dnames.data;
    dirIndexes = dixs.data;

    /*
     * fsm, psm and rpmfi assume the data is stored in a single allocation
     * block, until those assumptions are removed we need to jump through
     * a few hoops here and precalculate sizes etc
     */
    size = sizeof(*fileNames) * count;
    for (i = 0; i < count; i++)
	size += strlen(baseNames[i]) + strlen(dirNames[dirIndexes[i]]) + 1;

    fileNames = xmalloc(size);
    t = ((char *) fileNames) + (sizeof(*fileNames) * count);
    for (i = 0; i < count; i++) {
	fileNames[i] = t;
	t = stpcpy( stpcpy(t, dirNames[dirIndexes[i]]), baseNames[i]);
	*t++ = '\0';
    }
    rpmtdFreeData(&bnames);
    rpmtdFreeData(&dnames);
    rpmtdFreeData(&dixs);

    *fnp = fileNames;
    *fcp = count;
}

static int filedepTag(Header h, rpmTag tagN, rpmtd td, headerGetFlags hgflags)
{
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, RPMFI_NOHEADER);
    rpmds ds = NULL;
    char **fdeps = NULL;
    int numfiles;
    char deptype = 'R';
    int fileix;
    int rc = 0;

    numfiles = rpmfiFC(fi);
    if (numfiles <= 0) {
	goto exit;
    }

    if (tagN == RPMTAG_PROVIDENAME)
	deptype = 'P';
    else if (tagN == RPMTAG_REQUIRENAME)
	deptype = 'R';

    ds = rpmdsNew(h, tagN, 0);
    fdeps = xmalloc(numfiles * sizeof(*fdeps));

    while ((fileix = rpmfiNext(fi)) >= 0) {
	ARGV_t deps = NULL;
	const uint32_t * ddict = NULL;
	int ndx = rpmfiFDepends(fi, &ddict);
	if (ddict != NULL) {
	    while (ndx-- > 0) {
		const char * DNEVR;
		unsigned dix = *ddict++;
		char mydt = ((dix >> 24) & 0xff);
		if (mydt != deptype)
		    continue;
		dix &= 0x00ffffff;
		(void) rpmdsSetIx(ds, dix-1);
		if (rpmdsNext(ds) < 0)
		    continue;
		DNEVR = rpmdsDNEVR(ds);
		if (DNEVR != NULL) {
		    argvAdd(&deps, DNEVR + 2);
		}
	    }
	}
	fdeps[fileix] = deps ? argvJoin(deps, " ") : xstrdup("");
	argvFree(deps);
    }
    td->data = fdeps;
    td->count = numfiles;
    td->flags = RPMTD_ALLOCED | RPMTD_PTR_ALLOCED;
    td->type = RPM_STRING_ARRAY_TYPE;
    rc = 1;

exit:
    fi = rpmfiFree(fi);
    ds = rpmdsFree(ds);
    return rc;
}

/**
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int fsnamesTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    const char ** list;

    if (rpmGetFilesystemList(&list, &(td->count)))
	return 0;

    td->type = RPM_STRING_ARRAY_TYPE;
    td->data = list;

    return 1; 
}

/**
 * Retrieve install prefixes.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int instprefixTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    struct rpmtd_s prefixes;
    int flags = HEADERGET_MINMEM;

    if (headerGet(h, RPMTAG_INSTALLPREFIX, td, flags)) {
	return 1;
    } else if (headerGet(h, RPMTAG_INSTPREFIXES, &prefixes, flags)) {
	/* only return the first prefix of the array */
	td->type = RPM_STRING_TYPE;
	td->data = xstrdup(rpmtdGetString(&prefixes));
	td->flags = RPMTD_ALLOCED;
	rpmtdFreeData(&prefixes);
	return 1;
    }

    return 0;
}

/**
 * Retrieve mounted file system space.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int fssizesTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    struct rpmtd_s fsizes, fnames;
    const char ** filenames = NULL;
    rpm_loff_t * filesizes = NULL;
    rpm_loff_t * usages = NULL;
    rpm_count_t numFiles = 0;

    if (headerGet(h, RPMTAG_LONGFILESIZES, &fsizes, HEADERGET_EXT)) {
	filesizes = fsizes.data;
	headerGet(h, RPMTAG_FILENAMES, &fnames, HEADERGET_EXT);
	filenames = fnames.data;
	numFiles = rpmtdCount(&fnames);
    }
	
    if (rpmGetFilesystemList(NULL, &(td->count)))
	return 0;

    td->type = RPM_INT64_TYPE;
    td->flags = RPMTD_ALLOCED;

    if (filenames == NULL) {
	usages = xcalloc((td->count), sizeof(usages));
	td->data = usages;

	return 1;
    }

    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 0;

    td->data = usages;

    rpmtdFreeData(&fnames);
    rpmtdFreeData(&fsizes);

    return 1;
}

/**
 * Retrieve trigger info.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int triggercondsTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    uint32_t * indices;
    int i, j;
    char ** conds;
    struct rpmtd_s nametd, indextd, flagtd, versiontd, scripttd;
    int hgeflags = HEADERGET_MINMEM;

    if (!headerGet(h, RPMTAG_TRIGGERNAME, &nametd, hgeflags)) {
	return 0;
    }

    headerGet(h, RPMTAG_TRIGGERINDEX, &indextd, hgeflags);
    headerGet(h, RPMTAG_TRIGGERFLAGS, &flagtd, hgeflags);
    headerGet(h, RPMTAG_TRIGGERVERSION, &versiontd, hgeflags);
    headerGet(h, RPMTAG_TRIGGERSCRIPTS, &scripttd, hgeflags);

    td->type = RPM_STRING_ARRAY_TYPE;
    td->flags = RPMTD_ALLOCED | RPMTD_PTR_ALLOCED;
    td->data = conds = xmalloc(sizeof(*conds) * rpmtdCount(&scripttd));
    td->count = rpmtdCount(&scripttd);

    indices = indextd.data;

    while ((i = rpmtdNext(&scripttd)) >= 0) {
	rpm_flag_t *flag;
	char *flagStr, *item;
	ARGV_t items = NULL;

	rpmtdInit(&nametd); rpmtdInit(&flagtd); rpmtdInit(&versiontd);
	while ((j = rpmtdNext(&nametd)) >= 0) {
	    /* flag and version arrays match name array size always */
	    rpmtdNext(&flagtd); rpmtdNext(&versiontd);

	    if (indices[j] != i)
		continue;

	    flag = rpmtdGetUint32(&flagtd);
	    if (flag && *flag & RPMSENSE_SENSEMASK) {
		flagStr = rpmtdFormat(&flagtd, RPMTD_FORMAT_DEPFLAGS, NULL);
		rasprintf(&item, "%s %s %s", rpmtdGetString(&nametd),
					     flagStr,
					     rpmtdGetString(&versiontd));
		free(flagStr);
	    } else {
		item = xstrdup(rpmtdGetString(&nametd));
	    }

	    argvAdd(&items, item);
	    free(item);
	}

	conds[i] = argvJoin(items, ", ");
	argvFree(items);
    }

    rpmtdFreeData(&nametd);
    rpmtdFreeData(&versiontd);
    rpmtdFreeData(&flagtd);
    rpmtdFreeData(&indextd);
    rpmtdFreeData(&scripttd);
    return 1;
}

/**
 * Retrieve trigger type info.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int triggertypeTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    int i;
    char ** conds;
    struct rpmtd_s indices, flags, scripts;

    if (!headerGet(h, RPMTAG_TRIGGERINDEX, &indices, HEADERGET_MINMEM)) {
	return 0;
    }

    headerGet(h, RPMTAG_TRIGGERFLAGS, &flags, HEADERGET_MINMEM);
    headerGet(h, RPMTAG_TRIGGERSCRIPTS, &scripts, HEADERGET_MINMEM);

    td->flags = RPMTD_ALLOCED | RPMTD_PTR_ALLOCED;
    td->count = rpmtdCount(&scripts);
    td->data = conds = xmalloc(sizeof(*conds) * td->count);
    td->type = RPM_STRING_ARRAY_TYPE;

    while ((i = rpmtdNext(&scripts)) >= 0) {
	rpm_flag_t *flag;
	rpmtdInit(&indices); rpmtdInit(&flags);

	while (rpmtdNext(&indices) >= 0 && rpmtdNext(&flags) >= 0) {
	    if (*rpmtdGetUint32(&indices) != i) 
		continue;

	    flag = rpmtdGetUint32(&flags);
	    if (*flag & RPMSENSE_TRIGGERPREIN)
		conds[i] = xstrdup("prein");
	    else if (*flag & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (*flag & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else if (*flag & RPMSENSE_TRIGGERPOSTUN)
		conds[i] = xstrdup("postun");
	    else
		conds[i] = xstrdup("");
	    break;
	}
    }
    rpmtdFreeData(&indices);
    rpmtdFreeData(&flags);
    rpmtdFreeData(&scripts);

    return 1;
}

/**
 * Retrieve file paths.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int filenamesTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    rpmfiBuildFNames(h, RPMTAG_BASENAMES, 
		     (const char ***) &(td->data), &(td->count));
    if (td->data) {
	td->type = RPM_STRING_ARRAY_TYPE;
	td->flags = RPMTD_ALLOCED;
    }
    return (td->data != NULL); 
}

/**
 * Retrieve original file paths (wrt relocation).
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int origfilenamesTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    rpmfiBuildFNames(h, RPMTAG_ORIGBASENAMES, 
		     (const char ***) &(td->data), &(td->count));
    if (td->data) {
	td->type = RPM_STRING_ARRAY_TYPE;
	td->flags = RPMTD_ALLOCED;
    }
    return (td->data != NULL); 
}
/**
 * Retrieve file classes.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int fileclassTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, RPMFI_NOHEADER);
    char **fclasses;
    int ix, numfiles;
    int rc = 0;

    numfiles = rpmfiFC(fi);
    if (numfiles <= 0) {
	goto exit;
    }

    fclasses = xmalloc(numfiles * sizeof(*fclasses));
    rpmfiInit(fi, 0);
    while ((ix = rpmfiNext(fi)) >= 0) {
	const char *fclass = rpmfiFClass(fi);
	fclasses[ix] = xstrdup(fclass ? fclass : "");
    }

    td->data = fclasses;
    td->count = numfiles;
    td->flags = RPMTD_ALLOCED | RPMTD_PTR_ALLOCED;
    td->type = RPM_STRING_ARRAY_TYPE;
    rc = 1;

exit:
    fi = rpmfiFree(fi);
    return rc; 
}

/**
 * Retrieve file provides.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int fileprovideTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return filedepTag(h, RPMTAG_PROVIDENAME, td, hgflags);
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int filerequireTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return filedepTag(h, RPMTAG_REQUIRENAME, td, hgflags);
}

/* I18N look aside diversions */

#if defined(ENABLE_NLS)
extern int _nl_msg_cat_cntr;	/* XXX GNU gettext voodoo */
#endif
static const char * const language = "LANGUAGE";

static const char * const _macro_i18ndomains = "%{?_i18ndomains}";

/**
 * Retrieve i18n text.
 * @param h		header
 * @param tag		tag
 * @retval td		tag data container
 * @return		1 on success
 */
static int i18nTag(Header h, rpmTag tag, rpmtd td, headerGetFlags hgflags)
{
    char * dstring = rpmExpand(_macro_i18ndomains, NULL);
    int rc;

    td->type = RPM_STRING_TYPE;
    td->data = NULL;
    td->count = 0;

    if (dstring && *dstring) {
	char *domain, *de;
	const char * langval;
	char * msgkey;
	const char * msgid;

	rasprintf(&msgkey, "%s(%s)", headerGetString(h, RPMTAG_NAME), 
		  rpmTagGetName(tag));

	/* change to en_US for msgkey -> msgid resolution */
	langval = getenv(language);
	(void) setenv(language, "en_US", 1);
#if defined(ENABLE_NLS)
        ++_nl_msg_cat_cntr;
#endif

	msgid = NULL;
	for (domain = dstring; domain != NULL; domain = de) {
	    de = strchr(domain, ':');
	    if (de) *de++ = '\0';
	    msgid = dgettext(domain, msgkey);
	    if (msgid != msgkey) break;
	}

	/* restore previous environment for msgid -> msgstr resolution */
	if (langval)
	    (void) setenv(language, langval, 1);
	else
	    unsetenv(language);
#if defined(ENABLE_NLS)
        ++_nl_msg_cat_cntr;
#endif

	if (domain && msgid) {
	    td->data = dgettext(domain, msgid);
	    td->data = xstrdup(td->data); /* XXX xstrdup has side effects. */
	    td->count = 1;
	    td->flags = RPMTD_ALLOCED;
	}
	dstring = _free(dstring);
	free(msgkey);
	if (td->data)
	    return 1;
    }

    dstring = _free(dstring);

    rc = headerGet(h, tag, td, HEADERGET_ALLOC);
    return rc;
}

/**
 * Retrieve summary text.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int summaryTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return i18nTag(h, RPMTAG_SUMMARY, td, hgflags);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int descriptionTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return i18nTag(h, RPMTAG_DESCRIPTION, td, hgflags);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int groupTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return i18nTag(h, RPMTAG_GROUP, td, hgflags);
}

/*
 * Helper to convert 32bit tag to 64bit version.
 * If header has new 64bit tag then just return the data,
 * otherwise convert 32bit old tag data to 64bit values.
 * For consistency, always return malloced data.
 */
static int get64(Header h, rpmtd td, rpmTag newtag, rpmTag oldtag)
{
    int rc;

    if (headerIsEntry(h, newtag)) {
	rc = headerGet(h, newtag, td, HEADERGET_ALLOC);
    } else {
	struct rpmtd_s olddata;
	uint32_t *d32 = NULL;
	uint64_t *d64 = NULL;

	headerGet(h, oldtag, &olddata, HEADERGET_MINMEM);
	if (rpmtdType(&olddata) == RPM_INT32_TYPE) {
	    td->type = RPM_INT64_TYPE;
	    td->count = olddata.count;
	    td->flags = RPMTD_ALLOCED;
	    td->data = xmalloc(sizeof(*d64) * td->count);
	    d64 = td->data;
	    while ((d32 = rpmtdNextUint32(&olddata))) {
		*d64++ = *d32;
	    }
	} 
	rpmtdFreeData(&olddata);
	rc = d64 ? 1 : 0;
    }

    return rc;
}

/**
 * Retrieve file sizes as 64bit regardless of how they're stored.
 * @param h		header
 * @retval td		tag data container
 * @return		1 on success
 */
static int longfilesizesTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return get64(h, td, RPMTAG_LONGFILESIZES, RPMTAG_FILESIZES);
}

static int longarchivesizeTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return get64(h, td, RPMTAG_LONGARCHIVESIZE, RPMTAG_ARCHIVESIZE);
}

static int longsizeTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return get64(h, td, RPMTAG_LONGSIZE, RPMTAG_SIZE);
}

static int longsigsizeTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return get64(h, td, RPMTAG_LONGSIGSIZE, RPMTAG_SIGSIZE);
}

static int numberTag(rpmtd td, uint32_t val)
{
    uint32_t *tval = xmalloc(sizeof(*tval));

    tval[0] = val;
    td->type = RPM_INT32_TYPE;
    td->count = 1;
    td->data = tval;
    td->flags = RPMTD_ALLOCED;
    return 1; /* this cannot fail */
}

static int dbinstanceTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return numberTag(td, headerGetInstance(h));
}

static int headercolorTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    rpm_color_t *fcolor, hcolor = 0;
    struct rpmtd_s fcolors;

    headerGet(h, RPMTAG_FILECOLORS, &fcolors, HEADERGET_MINMEM);
    while ((fcolor = rpmtdNextUint32(&fcolors)) != NULL) {
	hcolor |= *fcolor;
    }
    rpmtdFreeData(&fcolors);
    hcolor &= 0x0f;

    return numberTag(td, hcolor);
}

typedef enum nevraFlags_e {
    NEVRA_NAME		= (1 << 0),
    NEVRA_EPOCH		= (1 << 1),
    NEVRA_VERSION	= (1 << 2),
    NEVRA_RELEASE	= (1 << 3),
    NEVRA_ARCH		= (1 << 4)
} nevraFlags;

static int getNEVRA(Header h, rpmtd td, nevraFlags flags)
{
    const char *val = NULL;
    char *res = NULL;

    if ((flags & NEVRA_NAME)) {
	val = headerGetString(h, RPMTAG_NAME);
	if (val) rstrscat(&res, val, "-", NULL);
    }
    if ((flags & NEVRA_EPOCH)) {
	char *e = headerGetAsString(h, RPMTAG_EPOCH);
	if (e) rstrscat(&res, e, ":", NULL);
	free(e);
    }
    if ((flags & NEVRA_VERSION)) {
	val = headerGetString(h, RPMTAG_VERSION);
	if (val) rstrscat(&res, val, "-", NULL);
    }
    if ((flags & NEVRA_RELEASE)) {
	val = headerGetString(h, RPMTAG_RELEASE);
	if (val) rstrscat(&res, val, NULL);
    }
    if ((flags & NEVRA_ARCH)) {
	val = headerGetString(h, RPMTAG_ARCH);
	if (headerIsSource(h) && val == NULL) val = "src";
	if (val) rstrscat(&res, ".", val, NULL);
    }

    td->type = RPM_STRING_TYPE;
    td->data = res;
    td->count = 1;
    td->flags = RPMTD_ALLOCED;

    return 1;
}

static int evrTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return getNEVRA(h, td, NEVRA_EPOCH|NEVRA_VERSION|NEVRA_RELEASE);
}

static int nvrTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return getNEVRA(h, td, NEVRA_NAME|NEVRA_VERSION|NEVRA_RELEASE);
}

static int nvraTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return getNEVRA(h, td, NEVRA_NAME|NEVRA_VERSION|NEVRA_RELEASE|NEVRA_ARCH);
}

static int nevrTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return getNEVRA(h, td, NEVRA_NAME|NEVRA_EPOCH|NEVRA_VERSION|NEVRA_RELEASE);
}

static int nevraTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    return getNEVRA(h, td, NEVRA_NAME|NEVRA_EPOCH|NEVRA_VERSION|NEVRA_RELEASE|NEVRA_ARCH);
}

static int verboseTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    if (rpmIsVerbose()) {
	td->type = RPM_INT32_TYPE;
	td->count = 1;
	td->data = &(td->count);
	td->flags = RPMTD_NONE;
	return 1;
    } else {
	return 0;
    }
}

static int epochnumTag(Header h, rpmtd td, headerGetFlags hgflags)
{
    /* For consistency, always return malloced data */
    if (!headerGet(h, RPMTAG_EPOCH, td, HEADERGET_ALLOC)) {
	uint32_t *e = malloc(sizeof(*e));
	*e = 0;
	td->data = e;
	td->type = RPM_INT32_TYPE;
	td->count = 1;
	td->flags = RPMTD_ALLOCED;
    }
    td->tag = RPMTAG_EPOCHNUM;
    return 1;
}

void *rpmHeaderTagFunc(rpmTag tag)
{
    const struct headerTagFunc_s * ext;
    void *func = NULL;

    for (ext = rpmHeaderTagExtensions; ext->func != NULL; ext++) {
	if (ext->tag == tag) {
	    func = ext->func;
	    break;
	}
    }
    return func;
}

static const struct headerTagFunc_s rpmHeaderTagExtensions[] = {
    { RPMTAG_GROUP,		groupTag },
    { RPMTAG_DESCRIPTION,	descriptionTag },
    { RPMTAG_SUMMARY,		summaryTag },
    { RPMTAG_FILECLASS,		fileclassTag },
    { RPMTAG_FILENAMES,		filenamesTag },
    { RPMTAG_ORIGFILENAMES,	origfilenamesTag },
    { RPMTAG_FILEPROVIDE,	fileprovideTag },
    { RPMTAG_FILEREQUIRE,	filerequireTag },
    { RPMTAG_FSNAMES,		fsnamesTag },
    { RPMTAG_FSSIZES,		fssizesTag },
    { RPMTAG_INSTALLPREFIX,	instprefixTag },
    { RPMTAG_TRIGGERCONDS,	triggercondsTag },
    { RPMTAG_TRIGGERTYPE,	triggertypeTag },
    { RPMTAG_LONGFILESIZES,	longfilesizesTag },
    { RPMTAG_LONGARCHIVESIZE,	longarchivesizeTag },
    { RPMTAG_LONGSIZE,		longsizeTag },
    { RPMTAG_LONGSIGSIZE,	longsigsizeTag },
    { RPMTAG_DBINSTANCE,	dbinstanceTag },
    { RPMTAG_EVR,		evrTag },
    { RPMTAG_NVR,		nvrTag },
    { RPMTAG_NEVR,		nevrTag },
    { RPMTAG_NVRA,		nvraTag },
    { RPMTAG_NEVRA,		nevraTag },
    { RPMTAG_HEADERCOLOR,	headercolorTag },
    { RPMTAG_VERBOSE,		verboseTag },
    { RPMTAG_EPOCHNUM,		epochnumTag },
    { 0, 			NULL }
};

