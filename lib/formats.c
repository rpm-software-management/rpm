/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for %_i18ndomains */
#include "rpmpgp.h"
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

    if (type != RPM_INT32_TYPE)
	val = xstrdup(_("(not a number)"));
    else if (*item & RPMSENSE_TRIGGERIN)
	val = xstrdup("in");
    else
	val = xstrdup("un");
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

/**
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static /*@only@*/ char * armorFormat(int_32 type, const void * data, 
		/*@unused@*/ char * formatPrefix, int padding, int element)
	/*@*/
{
    const char * enc;
    const char * s;
    char * t;
    char * val;
    int atype;
    int lc, ns, nt;

    switch (type) {
    case RPM_BIN_TYPE:
	s = data;
	ns = element;
	atype = PGPARMOR_SIGNATURE;	/* XXX check pkt for signature */
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	enc = data;
	if (b64decode(enc, (void **)&s, &ns))
	    return xstrdup(_("(not base64)"));
	atype = PGPARMOR_PUBKEY;	/* XXX check pkt for pubkey */
	break;
    case RPM_NULL_TYPE:
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_I18NSTRING_TYPE:
    default:
	return xstrdup(_("(invalid type)"));
	/*@notreached@*/ break;
    }

    nt = ((ns + 2) / 3) * 4;
    /*@-globs@*/
    /* Add additional bytes necessary for eol string(s). */
    if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	lc = (nt + b64encode_chars_per_line - 1) / b64encode_chars_per_line;
       if (((nt + b64encode_chars_per_line - 1) % b64encode_chars_per_line) != 0)
        ++lc;
	nt += lc * strlen(b64encode_eolstr);
    }
    /*@=globs@*/

    nt += 512;	/* XXX slop for armor and crc */

    val = t = xmalloc(nt + padding + 1);
    *t = '\0';
    t = stpcpy(t, "-----BEGIN PGP ");
    t = stpcpy(t, pgpValStr(pgpArmorTbl, atype));
    /*@-globs@*/
    t = stpcpy( stpcpy(t, "-----\nVersion: rpm-"), RPMVERSION);
    /*@=globs@*/
    t = stpcpy(t, " (beecrypt-2.2.0)\n\n");

    if ((enc = b64encode(s, ns)) != NULL) {
	t = stpcpy(t, enc);
	enc = _free(enc);
	if ((enc = b64crc(s, ns)) != NULL) {
	    *t++ = '=';
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
    }
	
    t = stpcpy(t, "-----END PGP ");
    t = stpcpy(t, pgpValStr(pgpArmorTbl, atype));
    t = stpcpy(t, "-----\n");

    /*@-branchstate@*/
    if (s != data) s = _free(s);
    /*@=branchstate@*/

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
static /*@only@*/ char * base64Format(int_32 type, const void * data, 
		/*@unused@*/ char * formatPrefix, int padding, int element)
	/*@*/
{
    char * val;

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	const char * enc;
	char * t;
	int lc;
	int nt = ((element + 2) / 3) * 4;

	/*@-globs@*/
	/* Add additional bytes necessary for eol string(s). */
	if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	    lc = (nt + b64encode_chars_per_line - 1) / b64encode_chars_per_line;
        if (((nt + b64encode_chars_per_line - 1) % b64encode_chars_per_line) != 0)
            ++lc;
	    nt += lc * strlen(b64encode_eolstr);
	}
	/*@=globs@*/

	val = t = xmalloc(nt + padding + 1);

	*t = '\0';
	if ((enc = b64encode(data, element)) != NULL) {
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
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
static /*@only@*/ char * pgppktFormat(int_32 type, const void * data, 
	char * formatPrefix, int padding, int element)
		/*@modifies formatPrefix @*/
{
    char * val;

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
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
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fsnamesTag( /*@unused@*/ Header h, /*@out@*/ int_32 * type,
		/*@out@*/ void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals fileSystem, internalState @*/
	/*@modifies *type, *data, *count, *freeData,
		fileSystem, internalState @*/
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
 * Retrieve install prefixes.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
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
 * Retrieve mounted file system space.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fssizesTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext,
		fileSystem, internalState @*/
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
 * Retrieve trigger info.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
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
    int i, j, xx;
    char buf[5];

    if (!hge(h, RPMTAG_TRIGGERNAME, &tnt, (void **) &names, &numNames)) {
	*freeData = 0;
	return 0;
    }

    xx = hge(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, NULL);
    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERVERSION, &tvt, (void **) &versions, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (void **) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		/*@innercontinue@*/ continue;

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
 * Retrieve trigger type info.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
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
    int i, j, xx;
    int numScripts, numNames;

    if (!hge(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, &numNames)) {
	*freeData = 0;
	return 1;
    }

    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (void **) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		/*@innercontinue@*/ continue;

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
 * Retrieve file paths.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
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
/*@unchecked@*/
int _nl_msg_cat_cntr;	/* XXX GNU gettext voodoo */
/*@=exportlocal =exportheadervar@*/
/*@observer@*/ /*@unchecked@*/
static const char * language = "LANGUAGE";

/*@observer@*/ /*@unchecked@*/
static const char * _macro_i18ndomains =
		"%{?_i18ndomains:%{_i18ndomains}}";

/**
 * Retrieve i18n text.
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int i18nTag(Header h, int_32 tag, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
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
/*@i@*/	++_nl_msg_cat_cntr;

	msgid = NULL;
	/*@-branchstate@*/
	for (domain = dstring; domain != NULL; domain = de) {
	    de = strchr(domain, ':');
	    if (de) *de++ = '\0';
	    msgid = /*@-unrecog@*/ dgettext(domain, msgkey) /*@=unrecog@*/;
	    if (msgid != msgkey) break;
	}
	/*@=branchstate@*/

	/* restore previous environment for msgid -> msgstr resolution */
	if (langval)
	    (void) setenv(language, langval, 1);
	else
	    unsetenv(language);
/*@i@*/	++_nl_msg_cat_cntr;

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
 * Retrieve summary text.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int summaryTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
{
    return i18nTag(h, RPMTAG_SUMMARY, type, data, count, freeData);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int descriptionTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
{
    return i18nTag(h, RPMTAG_DESCRIPTION, type, data, count, freeData);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freeData	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
static int groupTag(Header h, /*@out@*/ rpmTagType * type,
		/*@out@*/ const void ** data, /*@out@*/ int_32 * count,
		/*@out@*/ int * freeData)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies *type, *data, *count, *freeData, rpmGlobalMacroContext @*/
{
    return i18nTag(h, RPMTAG_GROUP, type, data, count, freeData);
}

/*@-type@*/ /* FIX: cast? */
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
    { HEADER_EXT_FORMAT, "armor", { armorFormat } },
    { HEADER_EXT_FORMAT, "base64", { base64Format } },
#ifdef	NOTYET
    { HEADER_EXT_FORMAT, "pgppkt", { pgppktFormat } },
#endif
    { HEADER_EXT_FORMAT, "depflags", { depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags", { fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms", { permsFormat } },
    { HEADER_EXT_FORMAT, "permissions", { permsFormat } },
    { HEADER_EXT_FORMAT, "triggertype", { triggertypeFormat } },
    { HEADER_EXT_MORE, NULL, { (void *) headerDefaultFormats } }
} ;
/*@=type@*/
