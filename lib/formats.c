/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for %_i18ndomains */
#include "manifest.h"
#include "misc.h"
#include "debug.h"

/**
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * triggertypeFormat(int_32 type, const void * data, 
	/*@unused@*/ char * formatPrefix, /*@unused@*/ int padding,
	/*@unused@*/ int element)	/*@*/
{
    const int_32 * item = data;
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else if (*item & RPMSENSE_TRIGGERIN) {
	val = xstrdup("in");
    } else {
	val = xstrdup("un");
    }

    return val;
}

/**
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * permsFormat(int_32 type, const void * data, char * formatPrefix,
	int padding, /*@unused@*/ int element)
		/*@modifies formatPrefix @*/
{
    char * val;
    char * buf;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(15 + padding);
	strcat(formatPrefix, "s");
	buf = rpmPermsString(*((int_32 *) data));
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
	buf = _free(buf);
    }

    return val;
}

/**
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * fflagsFormat(int_32 type, const void * data, 
	char * formatPrefix, int padding, /*@unused@*/ int element)
		/*@modifies formatPrefix @*/
{
    char * val;
    char buf[15];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	buf[0] = '\0';
	if (anint & RPMFILE_DOC)
	    strcat(buf, "d");
	if (anint & RPMFILE_CONFIG)
	    strcat(buf, "c");
	if (anint & RPMFILE_SPECFILE)
	    strcat(buf, "s");
	if (anint & RPMFILE_MISSINGOK)
	    strcat(buf, "m");
	if (anint & RPMFILE_NOREPLACE)
	    strcat(buf, "n");
	if (anint & RPMFILE_GHOST)
	    strcat(buf, "g");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
    }

    return val;
}

#ifdef	NOTYET
/**
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * base64Format(int_32 type, const void * data, 
	char * formatPrefix, int padding, /*@unused@*/ int element)
		/*@modifies formatPrefix @*/
{
    char * val;
    char buf[10];

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	buf[0] = '\0';

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
    }

    return val;
}
#endif

/**
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * depflagsFormat(int_32 type, const void * data, 
	char * formatPrefix, int padding, /*@unused@*/ int element)
		/*@modifies formatPrefix @*/
{
    char * val;
    char buf[10];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	buf[0] = '\0';

	if (anint & RPMSENSE_LESS) 
	    strcat(buf, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(buf, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(buf, "=");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	/*@-formatconst@*/
	sprintf(val, formatPrefix, buf);
	/*@=formatconst@*/
    }

    return val;
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fsnamesTag( /*@unused@*/ Header h, /*@out@*/ int_32 * type,
	/*@out@*/ void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    const char ** list;

    if (rpmGetFilesystemList(&list, count)) {
	return 1;
    }

    *type = RPM_STRING_ARRAY_TYPE;
    *((const char ***) data) = list;

    *freeData = 0;

    return 0; 
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int instprefixTag(Header h, /*@null@*/ /*@out@*/ rpmTagType * type,
	/*@null@*/ /*@out@*/ const void ** data,
	/*@null@*/ /*@out@*/ int_32 * count,
	/*@null@*/ /*@out@*/ int * freeData)
		/*@modifies *type, *data, *freeData @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType ipt;
    char ** array;

    if (hge(h, RPMTAG_INSTALLPREFIX, type, (void **)data, count)) {
	if (freeData) *freeData = 0;
	return 0;
    } else if (hge(h, RPMTAG_INSTPREFIXES, &ipt, (void **) &array, count)) {
	if (data) *data = xstrdup(array[0]);
	if (freeData) *freeData = 1;
	if (type) *type = RPM_STRING_TYPE;
	array = hfd(array, ipt);
	return 0;
    } 

    return 1;
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fssizesTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char ** filenames;
    int_32 * filesizes;
    uint_32 * usages;
    int numFiles;

    if (!hge(h, RPMTAG_FILESIZES, NULL, (void **) &filesizes, &numFiles)) {
	filesizes = NULL;
	numFiles = 0;
	filenames = NULL;
    } else {
	rpmBuildFileList(h, &filenames, &numFiles);
    }

    if (rpmGetFilesystemList(NULL, count)) {
	return 1;
    }

    *type = RPM_INT32_TYPE;
    *freeData = 1;

    if (filenames == NULL) {
	usages = xcalloc((*count), sizeof(usages));
	*data = usages;

	return 0;
    }

    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 1;

    *data = usages;

    filenames = _free(filenames);

    return 0;
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggercondsTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tnt, tvt, tst;
    int_32 * indices, * flags;
    char ** names, ** versions;
    int numNames, numScripts;
    char ** conds, ** s;
    char * item, * flagsStr;
    char * chptr;
    int i, j;
    char buf[5];

    if (!hge(h, RPMTAG_TRIGGERNAME, &tnt, (void **) &names, &numNames)) {
	*freeData = 0;
	return 0;
    }

    (void) hge(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, NULL);
    (void) hge(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    (void) hge(h, RPMTAG_TRIGGERVERSION, &tvt, (void **) &versions, NULL);
    (void) hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (void **) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(char * ) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i) continue;

	    item = xmalloc(strlen(names[j]) + strlen(versions[j]) + 20);
	    if (flags[j] & RPMSENSE_SENSEMASK) {
		buf[0] = '%', buf[1] = '\0';
		flagsStr = depflagsFormat(RPM_INT32_TYPE, flags, buf, 0, j);
		sprintf(item, "%s %s %s", names[j], flagsStr, versions[j]);
		flagsStr = _free(flagsStr);
	    } else {
		strcpy(item, names[j]);
	    }

	    chptr = xrealloc(chptr, strlen(chptr) + strlen(item) + 5);
	    if (*chptr != '\0') strcat(chptr, ", ");
	    strcat(chptr, item);
	    item = _free(item);
	}

	conds[i] = chptr;
    }

    names = hfd(names, tnt);
    versions = hfd(versions, tvt);

    return 0;
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggertypeTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tst;
    int_32 * indices, * flags;
    const char ** conds;
    const char ** s;
    int i, j;
    int numScripts, numNames;

    if (!hge(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, &numNames)) {
	*freeData = 0;
	return 1;
    }

    (void) hge(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    (void) hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (void **) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(char * ) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i) continue;

	    if (flags[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else
		conds[i] = xstrdup("postun");
	    /*@innerbreak@*/ break;
	}
    }

    return 0;
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filenamesTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    *type = RPM_STRING_ARRAY_TYPE;

    rpmBuildFileList(h, (const char ***) data, count);
    *freeData = 1;

    *freeData = 0;	/* XXX WTFO? */

    return 0; 
}

/* I18N look aside diversions */

/*@-exportlocal -exportheadervar@*/
int _nl_msg_cat_cntr;	/* XXX GNU gettext voodoo */
/*@=exportlocal =exportheadervar@*/
/*@observer@*/ static const char * language = "LANGUAGE";

/*@observer@*/ static const char * _macro_i18ndomains =
		"%{?_i18ndomains:%{_i18ndomains}}";

/**
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int i18nTag(Header h, int_32 tag, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    char * dstring = rpmExpand(_macro_i18ndomains, NULL);
    int rc;

    *type = RPM_STRING_TYPE;
    *data = NULL;
    *count = 0;
    *freeData = 0;

    if (dstring && *dstring) {
	char *domain, *de;
	const char * langval;
	const char * msgkey;
	const char * msgid;

	{   const char * tn = tagName(tag);
	    const char * n;
	    char * mk;
	    (void) headerNVR(h, &n, NULL, NULL);
	    mk = alloca(strlen(n) + strlen(tn) + sizeof("()"));
	    sprintf(mk, "%s(%s)", n, tn);
	    msgkey = mk;
	}

	/* change to en_US for msgkey -> msgid resolution */
	langval = getenv(language);
	(void) setenv(language, "en_US", 1);
	++_nl_msg_cat_cntr;

	msgid = NULL;
	for (domain = dstring; domain != NULL; domain = de) {
	    de = strchr(domain, ':');
	    if (de) *de++ = '\0';
	    msgid = /*@-unrecog@*/ dgettext(domain, msgkey) /*@=unrecog@*/;
	    if (msgid != msgkey) break;
	}

	/* restore previous environment for msgid -> msgstr resolution */
	if (langval)
	    (void) setenv(language, langval, 1);
	else
	    unsetenv(language);
	++_nl_msg_cat_cntr;

	if (domain && msgid) {
	    *data = /*@-unrecog@*/ dgettext(domain, msgid) /*@=unrecog@*/;
	    *data = xstrdup(*data);	/* XXX xstrdup has side effects. */
	    *count = 1;
	    *freeData = 1;
	}
	dstring = _free(dstring);
	if (*data)
	    return 0;
    }

    dstring = _free(dstring);

    rc = hge(h, tag, type, (void **)data, count);

    if (rc && (*data) != NULL) {
	*data = xstrdup(*data);
	*freeData = 1;
	return 0;
    }

    *freeData = 0;
    *data = NULL;
    *count = 0;
    return 1;
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int summaryTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    return i18nTag(h, RPMTAG_SUMMARY, type, data, count, freeData);
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int descriptionTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    return i18nTag(h, RPMTAG_DESCRIPTION, type, data, count, freeData);
}

/**
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int groupTag(Header h, /*@out@*/ rpmTagType * type,
	/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
	/*@out@*/ int * freeData)
		/*@modifies *type, *data, *count, *freeData @*/
{
    return i18nTag(h, RPMTAG_GROUP, type, data, count, freeData);
}

const struct headerSprintfExtension_s rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_GROUP", { groupTag } },
    { HEADER_EXT_TAG, "RPMTAG_DESCRIPTION", { descriptionTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUMMARY", { summaryTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILENAMES", { filenamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES", { fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES", { fsnamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX", { instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS", { triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE", { triggertypeTag } },
#ifdef	NOTYET
    { HEADER_EXT_FORMAT, "base64", { base64Format } },
#endif
    { HEADER_EXT_FORMAT, "depflags", { depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags", { fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms", { permsFormat } },
    { HEADER_EXT_FORMAT, "permissions", { permsFormat } },
    { HEADER_EXT_FORMAT, "triggertype", { triggertypeFormat } },
    { HEADER_EXT_MORE, NULL, { (void *) headerDefaultFormats } }
} ;
