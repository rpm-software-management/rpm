/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmlib.h>		/* rpmGetFilesystem*() */
#include <rpm/rpmds.h>
#include <rpm/rpmmacro.h>	/* XXX for %_i18ndomains */
#include <rpm/rpmfi.h>
#include <rpm/rpmstring.h>

#include "rpmio/digest.h"
#include "lib/manifest.h"

#include "debug.h"


/**
 * Identify type of trigger.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	(unused)
 * @return		formatted string
 */
static char * triggertypeFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,
		int element)
{
    const int32_t * item = data;
    char * val;

    if (type != RPM_INT32_TYPE)
	val = xstrdup(_("(not a number)"));
    else if (*item & RPMSENSE_TRIGGERPREIN)
	val = xstrdup("prein");
    else if (*item & RPMSENSE_TRIGGERIN)
	val = xstrdup("in");
    else if (*item & RPMSENSE_TRIGGERUN)
	val = xstrdup("un");
    else if (*item & RPMSENSE_TRIGGERPOSTUN)
	val = xstrdup("postun");
    else
	val = xstrdup("");
    return val;
}

/**
 * Format file permissions for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * permsFormat(rpmTagType type, rpm_constdata_t data,
		char * formatPrefix, size_t padding, int element)
{
    char * val;
    char * buf;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(15 + padding);
	strcat(formatPrefix, "s");
	buf = rpmPermsString(*((const int32_t *) data));
	sprintf(val, formatPrefix, buf);
	buf = _free(buf);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * fflagsFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding, int element)
{
    char * val;
    char buf[15];
    rpmfileAttrs anint = *((const rpm_flag_t *) data);

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
	if (anint & RPMFILE_LICENSE)
	    strcat(buf, "l");
	if (anint & RPMFILE_README)
	    strcat(buf, "r");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

/**
 * Wrap a pubkey in ascii armor for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding	(unused)
 * @param element	no. bytes of binary data
 * @return		formatted string
 */
static char * armorFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,
		int element)
{
    const char * enc;
    const unsigned char * s;
    unsigned char * bs;
    char *val;
    size_t ns;
    int atype;

    switch (type) {
    case RPM_BIN_TYPE:
	s = data;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	ns = element;
	atype = PGPARMOR_SIGNATURE;	/* XXX check pkt for signature */
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	enc = data;
	if (b64decode(enc, (void **)&bs, &ns))
	    return xstrdup(_("(not base64)"));
	s = bs;
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
	break;
    }

    /* XXX this doesn't use padding directly, assumes enough slop in retval. */
    val = pgpArmorWrap(atype, s, ns);
    free(bs);
    return val;
}

/**
 * Encode binary data in base64 for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding
 * @param element
 * @return		formatted string
 */
static char * base64Format(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding, int element)
{
    char * val;

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	char * enc;
	char * t;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	size_t ns = element;
	size_t nt = 0;

	if ((enc = b64encode(data, ns, -1)) != NULL) {
		nt = strlen(enc);
	}

	val = t = xmalloc(nt + padding + 1);

	*t = '\0';
	if (enc != NULL) {
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
    }

    return val;
}

/**
 */
static size_t xmlstrlen(const char * s)
{
    size_t len = 0;
    int c;

    while ((c = *s++) != '\0')
    {
	switch (c) {
	case '<': case '>':	len += 4;	break;
	case '&':		len += 5;	break;
	default:		len += 1;	break;
	}
    }
    return len;
}

/**
 */
static char * xmlstrcpy(char * t, const char * s)
{
    char * te = t;
    int c;

    while ((c = *s++) != '\0') {
	switch (c) {
	case '<':	te = stpcpy(te, "&lt;");	break;
	case '>':	te = stpcpy(te, "&gt;");	break;
	case '&':	te = stpcpy(te, "&amp;");	break;
	default:	*te++ = c;			break;
	}
    }
    *te = '\0';
    return t;
}

/**
 * Wrap tag data in simple header xml markup.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * xmlFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,
		int element)
{
    const char * xtag = NULL;
    size_t nb;
    char * val, * bs = NULL;
    const char * s = NULL;
    char * t, * te;
    unsigned long anint = 0;
    int xx;

    switch (type) {
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
	s = data;
	xtag = "string";
	break;
    case RPM_BIN_TYPE:
    {	
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	size_t ns = element;
    	if ((bs = b64encode(data, ns, 0)) == NULL) {
    		/* XXX proper error handling would be better. */
    		bs = xcalloc(1, padding + (ns / 3) * 4 + 1);
    	}
	s = bs;
	xtag = "base64";
    }	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
	anint = *((const uint8_t *) data);
	break;
    case RPM_INT16_TYPE:
	anint = *((const uint16_t *) data);
	break;
    case RPM_INT32_TYPE:
	anint = *((const uint32_t *) data);
	break;
    case RPM_NULL_TYPE:
    case RPM_STRING_ARRAY_TYPE:
    default:
	return xstrdup(_("(invalid xml type)"));
	break;
    }

    if (s == NULL) {
	size_t tlen = 32;
	t = memset(alloca(tlen+1), 0, tlen+1);
	if (anint != 0)
	    xx = snprintf(t, tlen, "%lu", anint);
	s = t;
	xtag = "integer";
    }

    nb = xmlstrlen(s);
    if (nb == 0) {
	nb += strlen(xtag) + sizeof("\t</>");
	te = t = alloca(nb);
	te = stpcpy( stpcpy( stpcpy(te, "\t<"), xtag), "/>");
    } else {
	nb += 2 * strlen(xtag) + sizeof("\t<></>");
	te = t = alloca(nb);
	te = stpcpy( stpcpy( stpcpy(te, "\t<"), xtag), ">");
	te = xmlstrcpy(te, s);
	te += strlen(te);
	te = stpcpy( stpcpy( stpcpy(te, "</"), xtag), ">");
    }

    /* XXX s was malloc'd */
    if (!strcmp(xtag, "base64"))
	free(bs);

    nb += padding;
    val = xmalloc(nb+1);
    strcat(formatPrefix, "s");
    xx = snprintf(val, nb, formatPrefix, t);
    val[nb] = '\0';

    return val;
}

/**
 * Display signature fingerprint and time.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix	(unused)
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * pgpsigFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding,
		int element)
{
    char * val, * t;

    if (type != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	const uint8_t * pkt = (const uint8_t *) data;
	size_t pktlen = 0;
	unsigned int v = *pkt;
	pgpTag tag = 0;
	size_t plen;
	size_t hlen = 0;

	if (v & 0x80) {
	    if (v & 0x40) {
		tag = (v & 0x3f);
		plen = pgpLen(pkt+1, &hlen);
	    } else {
		tag = (v >> 2) & 0xf;
		plen = (1 << (v & 0x3));
		hlen = pgpGrab(pkt+1, plen);
	    }
	
	    pktlen = 1 + plen + hlen;
	}

	if (pktlen == 0 || tag != PGPTAG_SIGNATURE) {
	    val = xstrdup(_("(not an OpenPGP signature)"));
	} else {
	    pgpDig dig = pgpNewDig();
	    pgpDigParams sigp = &dig->signature;
	    size_t nb = 0;
	    char *tempstr = NULL;

	    (void) pgpPrtPkts(pkt, pktlen, dig, 0);

	    val = NULL;
	again:
	    nb += 100;
	    val = t = xrealloc(val, nb + 1);

	    switch (sigp->pubkey_algo) {
	    case PGPPUBKEYALGO_DSA:
		t = stpcpy(t, "DSA");
		break;
	    case PGPPUBKEYALGO_RSA:
		t = stpcpy(t, "RSA");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%d", sigp->pubkey_algo);
		t += strlen(t);
		break;
	    }
	    if (t + 5 >= val + nb)
		goto again;
	    *t++ = '/';
	    switch (sigp->hash_algo) {
	    case PGPHASHALGO_MD5:
		t = stpcpy(t, "MD5");
		break;
	    case PGPHASHALGO_SHA1:
		t = stpcpy(t, "SHA1");
		break;
	    default:
		(void) snprintf(t, nb - (t - val), "%d", sigp->hash_algo);
		t += strlen(t);
		break;
	    }
	    if (t + strlen (", ") + 1 >= val + nb)
		goto again;

	    t = stpcpy(t, ", ");

	    /* this is important if sizeof(int32_t) ! sizeof(time_t) */
	    {	time_t dateint = pgpGrab(sigp->time, sizeof(sigp->time));
		struct tm * tstruct = localtime(&dateint);
		if (tstruct)
 		    (void) strftime(t, (nb - (t - val)), "%c", tstruct);
	    }
	    t += strlen(t);
	    if (t + strlen (", Key ID ") + 1 >= val + nb)
		goto again;
	    t = stpcpy(t, ", Key ID ");
	    tempstr = pgpHexStr(sigp->signid, sizeof(sigp->signid));
	    if (t + strlen (tempstr) > val + nb)
		goto again;
	    t = stpcpy(t, tempstr);
	    free(tempstr);

	    dig = pgpFreeDig(dig);
	}
    }

    return val;
}

/**
 * Format dependency flags for display.
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element	(unused)
 * @return		formatted string
 */
static char * depflagsFormat(rpmTagType type, rpm_constdata_t data, 
		char * formatPrefix, size_t padding, int element)
{
    char * val;
    char buf[10];
    rpmsenseFlags anint;

    if (type != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	anint = *((const rpm_flag_t *) data);
	buf[0] = '\0';

	if (anint & RPMSENSE_LESS) 
	    strcat(buf, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(buf, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(buf, "=");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

/**
 * Retrieve mounted file system paths.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fsnamesTag( Header h, int32_t * type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    const char ** list;

    if (rpmGetFilesystemList(&list, count))
	return 1;

    if (type) *type = RPM_STRING_ARRAY_TYPE;
    if (data) *((const char ***) data) = list;
    if (freeData) *freeData = 0;

    return 0; 
}

/**
 * Retrieve install prefixes.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int instprefixTag(Header h, rpmTagType* type,
		rpm_data_t * data,
		rpm_count_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType ipt;
    char ** array;

    if (hge(h, RPMTAG_INSTALLPREFIX, type, (rpm_data_t *)data, count)) {
	if (freeData) *freeData = 0;
	return 0;
    } else if (hge(h, RPMTAG_INSTPREFIXES, &ipt, (rpm_data_t *) &array, count)) {
	if (type) *type = RPM_STRING_TYPE;
	if (data) *data = xstrdup(array[0]);
	if (freeData) *freeData = 1;
	array = hfd(array, ipt);
	return 0;
    }

    return 1;
}

/**
 * Retrieve mounted file system space.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fssizesTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
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

    if (rpmGetFilesystemList(NULL, count))
	return 1;

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
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggercondsTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    rpmTagType tnt, tvt, tst;
    int32_t * indices;
    char ** names, ** versions;
    rpm_count_t numNames, numScripts;
    rpm_count_t i, j;
    rpm_flag_t * flags;
    char ** conds, ** s;
    char * item, * flagsStr;
    char * chptr;
    int xx;
    char buf[5];

    if (!hge(h, RPMTAG_TRIGGERNAME, &tnt, (rpm_data_t *) &names, &numNames)) {
	*freeData = 0;
	return 0;
    }

    xx = hge(h, RPMTAG_TRIGGERINDEX, NULL, (rpm_data_t *) &indices, NULL);
    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (rpm_data_t *) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERVERSION, &tvt, (rpm_data_t *) &versions, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (rpm_data_t *) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i)
		continue;

	    item = xmalloc(strlen(names[j]) + strlen(versions[j]) + 20);
	    if (flags[j] & RPMSENSE_SENSEMASK) {
		buf[0] = '%', buf[1] = '\0';
		flagsStr = depflagsFormat(RPM_INT32_TYPE, flags, buf, 0, 0);
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
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int triggertypeTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
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
	*freeData = 0;
	return 1;
    }

    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (rpm_data_t *) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (rpm_data_t *) &s, &numScripts);
    s = hfd(s, tst);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(*conds) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
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
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filenamesTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFNames(h, RPMTAG_BASENAMES, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/**
 * Retrieve file classes.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fileclassTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFClasses(h, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/**
 * Retrieve file provides.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int fileprovideTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_PROVIDENAME, (const char ***) data, count);
    *freeData = 1;
    return 0; 
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int filerequireTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    *type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_REQUIRENAME, (const char ***) data, count);
    *freeData = 1;
    return 0; 
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
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int i18nTag(Header h, rpmTag tag, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
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
	    *data = dgettext(domain, msgid);
	    *data = xstrdup(*data);	/* XXX xstrdup has side effects. */
	    *count = 1;
	    *freeData = 1;
	}
	dstring = _free(dstring);
	free(msgkey);
	if (*data)
	    return 0;
    }

    dstring = _free(dstring);

    rc = hge(h, tag, type, data, count);

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
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int summaryTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    return i18nTag(h, RPMTAG_SUMMARY, type, data, count, freeData);
}

/**
 * Retrieve description text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int descriptionTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    return i18nTag(h, RPMTAG_DESCRIPTION, type, data, count, freeData);
}

/**
 * Retrieve group text.
 * @param h		header
 * @retval *type	tag type
 * @retval *data	tag value
 * @retval *count	no. of data items
 * @retval *freeData	data-was-malloc'ed indicator
 * @return		0 on success
 */
static int groupTag(Header h, rpmTagType* type,
		rpm_data_t * data, rpm_count_t * count,
		int * freeData)
{
    return i18nTag(h, RPMTAG_GROUP, type, data, count, freeData);
}

/* FIX: cast? */
const struct headerSprintfExtension_s rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_GROUP",		{ groupTag } },
    { HEADER_EXT_TAG, "RPMTAG_DESCRIPTION",	{ descriptionTag } },
    { HEADER_EXT_TAG, "RPMTAG_SUMMARY",		{ summaryTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILECLASS",	{ fileclassTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILENAMES",	{ filenamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEPROVIDE",	{ fileprovideTag } },
    { HEADER_EXT_TAG, "RPMTAG_FILEREQUIRE",	{ filerequireTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES",		{ fsnamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES",		{ fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX",	{ instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS",	{ triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE",	{ triggertypeTag } },
    { HEADER_EXT_FORMAT, "armor",		{ armorFormat } },
    { HEADER_EXT_FORMAT, "base64",		{ base64Format } },
    { HEADER_EXT_FORMAT, "pgpsig",		{ pgpsigFormat } },
    { HEADER_EXT_FORMAT, "depflags",		{ depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags",		{ fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms",		{ permsFormat } },
    { HEADER_EXT_FORMAT, "permissions",		{ permsFormat } },
    { HEADER_EXT_FORMAT, "triggertype",		{ triggertypeFormat } },
    { HEADER_EXT_FORMAT, "xml",			{ xmlFormat } },
    { HEADER_EXT_MORE, NULL,		{ (void *) headerDefaultFormats } }
} ;
