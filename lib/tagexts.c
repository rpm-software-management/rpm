/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmlib.h>		/* rpmGetFilesystem*() */
#include <rpm/rpmmacro.h>	/* XXX for %_i18ndomains */
#include <rpm/rpmfi.h>
#include <rpm/rpmstring.h>

#include "debug.h"

struct headerTagFunc_s {
    rpmTag tag;		/*!< Tag of extension. */
    void *func;		/*!< Pointer to formatter function. */	
};

/* forward declarations */
static const struct headerTagFunc_s rpmHeaderTagExtensions[];

void *rpmHeaderTagFunc(rpmTag tag);

static int filedepTag(Header h, rpmTag tagN, rpmtd td)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    rpmds ds = NULL;
    char **fdeps = NULL;
    int numfiles;
    char deptype = 'R';
    int fileix;

    numfiles = rpmfiFC(fi);
    if (numfiles <= 0) {
	goto exit;
    }

    if (tagN == RPMTAG_PROVIDENAME)
	deptype = 'P';
    else if (tagN == RPMTAG_REQUIRENAME)
	deptype = 'R';

    ds = rpmdsNew(h, tagN, scareMem);
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
		    argvAdd(&deps, DNEVR);
		}
	    }
	}
	fdeps[fileix] = deps ? argvJoin(deps, " ") : xstrdup("");
	argvFree(deps);
    }
    td->data = fdeps;
    td->count = numfiles;
    td->flags = RPMTD_ALLOCED | RPMTD_PTR_ALLOCED;

exit:
    td->type = RPM_STRING_ARRAY_TYPE;
    fi = rpmfiFree(fi);
    ds = rpmdsFree(ds);
    return 0;
}

/**
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int fsnamesTag(Header h, rpmtd td)
{
    const char ** list;

    if (rpmGetFilesystemList(&list, &(td->count)))
	return 1;

    td->type = RPM_STRING_ARRAY_TYPE;
    td->data = list;

    return 0; 
}

/**
 * Retrieve install prefixes.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int instprefixTag(Header h, rpmtd td)
{
    struct rpmtd_s prefixes;
    int flags = HEADERGET_MINMEM;

    if (headerGet(h, RPMTAG_INSTALLPREFIX, td, flags)) {
	return 0;
    } else if (headerGet(h, RPMTAG_INSTPREFIXES, &prefixes, flags)) {
	/* only return the first prefix of the array */
	td->type = RPM_STRING_TYPE;
	td->data = xstrdup(rpmtdGetString(&prefixes));
	td->flags = RPMTD_ALLOCED;
	rpmtdFreeData(&prefixes);
	return 0;
    }

    return 1;
}

/**
 * Retrieve mounted file system space.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int fssizesTag(Header h, rpmtd td)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char ** filenames;
    rpm_off_t * filesizes;
    rpm_off_t * usages;
    rpm_count_t numFiles;

    if (!hge(h, RPMTAG_FILESIZES, NULL, (rpm_data_t *) &filesizes, &numFiles)) {
	filesizes = NULL;
	numFiles = 0;
	filenames = NULL;
    } else {
	rpmfiBuildFNames(h, RPMTAG_BASENAMES, &filenames, &numFiles);
    }

    if (rpmGetFilesystemList(NULL, &(td->count)))
	return 1;

    td->type = RPM_INT32_TYPE;
    td->flags = RPMTD_ALLOCED;

    if (filenames == NULL) {
	usages = xcalloc((td->count), sizeof(usages));
	td->data = usages;

	return 0;
    }

    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 1;

    td->data = usages;

    filenames = _free(filenames);

    return 0;
}

/**
 * Retrieve trigger info.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int triggercondsTag(Header h, rpmtd td)
{
    uint32_t * indices;
    int i, j;
    char ** conds;
    struct rpmtd_s nametd, indextd, flagtd, versiontd, scripttd;
    int hgeflags = HEADERGET_MINMEM;

    td->type = RPM_STRING_ARRAY_TYPE;
    if (!headerGet(h, RPMTAG_TRIGGERNAME, &nametd, hgeflags)) {
	return 0;
    }

    headerGet(h, RPMTAG_TRIGGERINDEX, &indextd, hgeflags);
    headerGet(h, RPMTAG_TRIGGERFLAGS, &flagtd, hgeflags);
    headerGet(h, RPMTAG_TRIGGERVERSION, &versiontd, hgeflags);
    headerGet(h, RPMTAG_TRIGGERSCRIPTS, &scripttd, hgeflags);

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
    return 0;
}

/**
 * Retrieve trigger type info.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int triggertypeTag(Header h, rpmtd td)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tst;
    int32_t * indices;
    const char ** conds;
    const char ** s;
    int xx;
    rpm_count_t numScripts, numNames;
    rpm_count_t i, j;
    rpm_flag_t * flags;

    if (!hge(h, RPMTAG_TRIGGERINDEX, NULL, (rpm_data_t *) &indices, &numNames)) {
	return 1;
    }

    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (rpm_data_t *) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (rpm_data_t *) &s, &numScripts);
    s = hfd(s, tst);

    td->flags = RPMTD_ALLOCED | RPMTD_PTR_ALLOCED;
    td->data = conds = xmalloc(sizeof(*conds) * numScripts);
    td->count = numScripts;
    td->type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		continue;

	    if (flags[j] & RPMSENSE_TRIGGERPREIN)
		conds[i] = xstrdup("prein");
	    else if (flags[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else if (flags[j] & RPMSENSE_TRIGGERPOSTUN)
		conds[i] = xstrdup("postun");
	    else
		conds[i] = xstrdup("");
	    break;
	}
    }

    return 0;
}

/**
 * Retrieve file paths.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int filenamesTag(Header h, rpmtd td)
{
    td->type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFNames(h, RPMTAG_BASENAMES, 
		     (const char ***) &(td->data), &(td->count));
    td->flags = RPMTD_ALLOCED;
    return 0; 
}

/**
 * Retrieve file classes.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int fileclassTag(Header h, rpmtd td)
{
    int scareMem = 0;
    rpmfi fi = rpmfiNew(NULL, h, RPMTAG_BASENAMES, scareMem);
    char **fclasses;
    int ix, numfiles;

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

exit:
    td->type = RPM_STRING_ARRAY_TYPE;
    fi = rpmfiFree(fi);
    return 0; 
}

/**
 * Retrieve file provides.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int fileprovideTag(Header h, rpmtd td)
{
    return filedepTag(h, RPMTAG_PROVIDENAME, td);
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int filerequireTag(Header h, rpmtd td)
{
    return filedepTag(h, RPMTAG_REQUIRENAME, td);
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
 * @return		0 on success
 */
static int i18nTag(Header h, rpmTag tag, rpmtd td)
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
	const char * n;
	int xx;

	xx = headerNVR(h, &n, NULL, NULL);
	rasprintf(&msgkey, "%s(%s)", n, rpmTagGetName(tag));

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
	    return 0;
    }

    dstring = _free(dstring);

    rc = headerGet(h, tag, td, HEADERGET_DEFAULT);
    /* XXX fix the mismatch between headerGet and tag format returns */
    return rc ? 0 : 1;
}

/**
 * Retrieve summary text.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int summaryTag(Header h, rpmtd td)
{
    return i18nTag(h, RPMTAG_SUMMARY, td);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int descriptionTag(Header h, rpmtd td)
{
    return i18nTag(h, RPMTAG_DESCRIPTION, td);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int groupTag(Header h, rpmtd td)
{
    return i18nTag(h, RPMTAG_GROUP, td);
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
    { RPMTAG_FILEPROVIDE,	fileprovideTag },
    { RPMTAG_FILEREQUIRE,	filerequireTag },
    { RPMTAG_FSNAMES,		fsnamesTag },
    { RPMTAG_FSSIZES,		fssizesTag },
    { RPMTAG_INSTALLPREFIX,	instprefixTag },
    { RPMTAG_TRIGGERCONDS,	triggercondsTag },
    { RPMTAG_TRIGGERTYPE,	triggertypeTag },
    { 0, 			NULL }
};

