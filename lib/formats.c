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

/** \ingroup header
 * Define header tag output formats.
 */

struct headerFormatFunc_s {
    rpmtdFormats fmt;	/*!< Value of extension */
    const char *name;	/*!< Name of extension. */
    void *func;		/*!< Pointer to formatter function. */	
};

struct headerTagFunc_s {
    rpmTag tag;		/*!< Tag of extension. */
    void *func;		/*!< Pointer to formatter function. */	
};

/* forward declarations */
static const struct headerFormatFunc_s rpmHeaderFormats[];
static const struct headerTagFunc_s rpmHeaderTagExtensions[];

void *rpmHeaderFormatFuncByName(const char *fmt);
void *rpmHeaderFormatFuncByValue(rpmtdFormats fmt);
void *rpmHeaderTagFunc(rpmTag tag);

/**
 * barebones string representation with no extra formatting
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * stringFormat(rpmtd td, char *formatPrefix, size_t padding)
{
    const char *str = NULL;
    char *val = NULL, *buf = NULL;
    size_t need;

    switch (rpmtdType(td)) {
	case RPM_INT8_TYPE:
	case RPM_CHAR_TYPE:
	    need = 10 + padding + 20; /* we can do better, just for now ... */
	    val = xmalloc(need);
	    strcat(formatPrefix, "hhu");
	    sprintf(val, formatPrefix, *rpmtdGetChar(td));
	    break;
	case RPM_INT16_TYPE:
	    need = 10 + padding + 20; /* we can do better, just for now ... */
	    val = xmalloc(need);
	    strcat(formatPrefix, "hu");
	    sprintf(val, formatPrefix, *rpmtdGetUint16(td));
	    break;
	case RPM_INT32_TYPE:
	    need = 10 + padding + 20; /* we can do better, just for now ... */
	    val = xmalloc(need);
	    strcat(formatPrefix, "u");
	    sprintf(val, formatPrefix, *rpmtdGetUint32(td));
	    break;
	case RPM_STRING_TYPE:
	case RPM_STRING_ARRAY_TYPE:
	case RPM_I18NSTRING_TYPE:
	    str = rpmtdGetString(td);
	    val = xmalloc(strlen(str) + padding + 1);
	    strcat(formatPrefix, "s");
	    sprintf(val, formatPrefix, str);
	    break;
	case RPM_BIN_TYPE:
	    buf = pgpHexStr(td->data, td->count);
	    val = xmalloc(strlen(buf) + padding + 1);
	    strcat(formatPrefix, "s");
	    sprintf(val, formatPrefix, buf);
	    free(buf);
	    break;
	default:
	    val = xstrdup("(unknown type)");
	    break;
    }
    return val;
}

/**
 * octalFormat.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * octalFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val;

    if (rpmtdType(td) != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "o");
	sprintf(val, formatPrefix, *rpmtdGetUint32(td));
    }

    return val;
}

/**
 * hexFormat.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * hexFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val;

    if (rpmtdType(td) != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(20 + padding);
	strcat(formatPrefix, "x");
	sprintf(val, formatPrefix, *rpmtdGetUint32(td));
    }

    return val;
}

/**
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * realDateFormat(rpmtd td, char * formatPrefix, size_t padding,
		const char * strftimeFormat)
{
    char * val;

    if (rpmtdType(td) != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	struct tm * tstruct;
	char buf[50];

	val = xmalloc(50 + padding);
	strcat(formatPrefix, "s");

	/* this is important if sizeof(rpm_time_t) ! sizeof(time_t) */
	{   rpm_time_t *rt = rpmtdGetUint32(td);
	    time_t dateint = *rt;
	    tstruct = localtime(&dateint);
	}
	buf[0] = '\0';
	if (tstruct)
	    (void) strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

/**
 * Format a date.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * dateFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    return realDateFormat(td, formatPrefix, padding, _("%c"));
}

/**
 * Format a day.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * dayFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    return realDateFormat(td, formatPrefix, padding, _("%a %b %d %Y"));
}

/**
 * Return shell escape formatted data.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * shescapeFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * result, * dst, * src;

    if (rpmtdType(td) == RPM_INT32_TYPE) {
	result = xmalloc(padding + 20);
	strcat(formatPrefix, "d");
	sprintf(result, formatPrefix, *rpmtdGetUint32(td));
    } else {
	char *buf = NULL;
	strcat(formatPrefix, "s");
	rasprintf(&buf, formatPrefix, rpmtdGetString(td));

	result = dst = xmalloc(strlen(buf) * 4 + 3);
	*dst++ = '\'';
	for (src = buf; *src != '\0'; src++) {
	    if (*src == '\'') {
		*dst++ = '\'';
		*dst++ = '\\';
		*dst++ = '\'';
		*dst++ = '\'';
	    } else {
		*dst++ = *src;
	    }
	}
	*dst++ = '\'';
	*dst = '\0';
	free(buf);
    }

    return result;
}


/**
 * Identify type of trigger.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * triggertypeFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    const uint32_t * item = rpmtdGetUint32(td);
    char * val;

    if (rpmtdType(td) != RPM_INT32_TYPE)
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
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * permsFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val;
    char * buf;

    if (rpmtdType(td) != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = xmalloc(15 + padding);
	strcat(formatPrefix, "s");
	buf = rpmPermsString(*rpmtdGetUint32(td));
	sprintf(val, formatPrefix, buf);
	buf = _free(buf);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * fflagsFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val;
    char buf[15];

    if (rpmtdType(td) != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	rpm_flag_t *rf = rpmtdGetUint32(td);
	rpmfileAttrs anint = *rf;
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
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * armorFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    const char * enc;
    const unsigned char * s;
    unsigned char * bs = NULL;
    char *val;
    size_t ns;
    int atype;

    switch (rpmtdType(td)) {
    case RPM_BIN_TYPE:
	s = td->data;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	ns = td->count;
	atype = PGPARMOR_SIGNATURE;	/* XXX check pkt for signature */
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	enc = rpmtdGetString(td);
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
    if (atype == PGPARMOR_PUBKEY) {
    	free(bs);
    }
    return val;
}

/**
 * Encode binary data in base64 for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * base64Format(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val;

    if (rpmtdType(td) != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	char * enc;
	char * t;
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	size_t ns = td->count;
	size_t nt = 0;

	if ((enc = b64encode(td->data, ns, -1)) != NULL) {
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
 * Wrap tag data in simple header xml markup.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * xmlFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    const char *xtag = NULL;
    size_t nb = 0;
    char *val;
    char *s = NULL;
    char *t = NULL;
    rpmtdFormats fmt = RPMTD_FORMAT_STRING;
    int xx;

    switch (rpmtdType(td)) {
    case RPM_I18NSTRING_TYPE:
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	xtag = "string";
	break;
    case RPM_BIN_TYPE:
	fmt = RPMTD_FORMAT_BASE64;
	xtag = "base64";
	break;
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
	xtag = "integer";
	break;
    case RPM_NULL_TYPE:
    default:
	return xstrdup(_("(invalid xml type)"));
	break;
    }

    /* XXX TODO: handle errors */
    s = rpmtdFormat(td, fmt, NULL);

    if (s[0] == '\0') {
	t = rstrscat(NULL, "\t<", xtag, "/>", NULL);
    } else {
	char *new_s = NULL;
	size_t i, s_size = strlen(s);
	
	for (i=0; i<s_size; i++) {
	    switch (s[i]) {
		case '<':	rstrcat(&new_s, "&lt;");	break;
		case '>':	rstrcat(&new_s, "&gt;");	break;
		case '&':	rstrcat(&new_s, "&amp;");	break;
		default: {
		    char c[2] = " ";
		    *c = s[i];
		    rstrcat(&new_s, c);
		    break;
		}
	    }
	}

	t = rstrscat(NULL, "\t<", xtag, ">", new_s, "</", xtag, ">", NULL);
	free(new_s);
    }
    free(s);

    nb += strlen(t)+padding+1;
    val = xmalloc(nb+1);
    strcat(formatPrefix, "s");
    xx = snprintf(val, nb, formatPrefix, t);
    free(t);
    val[nb] = '\0';

    return val;
}

/**
 * Display signature fingerprint and time.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * pgpsigFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val, * t;

    if (rpmtdType(td) != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	const uint8_t * pkt = td->data;
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
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @param padding	no. additional bytes needed by format string
 * @return		formatted string
 */
static char * depflagsFormat(rpmtd td, char * formatPrefix, size_t padding)
{
    char * val;
    char buf[10];
    rpmsenseFlags anint;

    if (rpmtdType(td) != RPM_INT32_TYPE) {
	val = xstrdup(_("(not a number)"));
    } else {
	rpm_flag_t *rf = rpmtdGetUint32(td);
	anint = *rf;
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
    td->freeData = 0;

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
	td->data = rpmtdToString(&prefixes);
	td->freeData = 1;
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
    td->freeData = 1;

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
    /* XXX out of service  for now.. */
#if 0
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
	td->freeData = 0;
	return 0;
    }

    xx = hge(h, RPMTAG_TRIGGERINDEX, NULL, (rpm_data_t *) &indices, NULL);
    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (rpm_data_t *) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERVERSION, &tvt, (rpm_data_t *) &versions, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (rpm_data_t *) &s, &numScripts);
    s = hfd(s, tst);

    td->freeData = 1;
    td->data = conds = xmalloc(sizeof(*conds) * numScripts);
    td->count = numScripts;
    td->type = RPM_STRING_ARRAY_TYPE;
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
#endif
    return 1;
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
	td->freeData = 0;
	return 1;
    }

    xx = hge(h, RPMTAG_TRIGGERFLAGS, NULL, (rpm_data_t *) &flags, NULL);
    xx = hge(h, RPMTAG_TRIGGERSCRIPTS, &tst, (rpm_data_t *) &s, &numScripts);
    s = hfd(s, tst);

    td->freeData = 1;
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
    td->freeData = 1;
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
    td->type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFClasses(h, (const char ***) &(td->data), &(td->count));
    td->freeData = 1;
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
    td->type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_PROVIDENAME, 
		    (const char ***) &(td->data), &(td->count));
    td->freeData = 1;
    return 0; 
}

/**
 * Retrieve file requires.
 * @param h		header
 * @retval td		tag data container
 * @return		0 on success
 */
static int filerequireTag(Header h, rpmtd td)
{
    td->type = RPM_STRING_ARRAY_TYPE;
    rpmfiBuildFDeps(h, RPMTAG_REQUIRENAME, 
		    (const char ***) &(td->data), &(td->count));
    td->freeData = 1;
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
    td->freeData = 0;

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
	    td->freeData = 1;
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

void *rpmHeaderFormatFuncByName(const char *fmt)
{
    const struct headerFormatFunc_s * ext;
    void *func = NULL;

    for (ext = rpmHeaderFormats; ext->name != NULL; ext++) {
	if (!strcmp(ext->name, fmt)) {
	    func = ext->func;
	    break;
	}
    }
    return func;
}

void *rpmHeaderFormatFuncByValue(rpmtdFormats fmt)
{
    const struct headerFormatFunc_s * ext;
    void *func = NULL;

    for (ext = rpmHeaderFormats; ext->name != NULL; ext++) {
	if (fmt == ext->fmt) {
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

static const struct headerFormatFunc_s rpmHeaderFormats[] = {
    { RPMTD_FORMAT_STRING,	"string",	stringFormat },
    { RPMTD_FORMAT_ARMOR,	"armor",	armorFormat },
    { RPMTD_FORMAT_BASE64,	"base64",	base64Format },
    { RPMTD_FORMAT_PGPSIG,	"pgpsig",	pgpsigFormat },
    { RPMTD_FORMAT_DEPFLAGS,	"depflags",	depflagsFormat },
    { RPMTD_FORMAT_FFLAGS,	"fflags",	fflagsFormat },
    { RPMTD_FORMAT_PERMS,	"perms",	permsFormat },
    { RPMTD_FORMAT_PERMS,	"permissions",	permsFormat },
    { RPMTD_FORMAT_TRIGGERTYPE,	"triggertype",	triggertypeFormat },
    { RPMTD_FORMAT_XML,		"xml",		xmlFormat },
    { RPMTD_FORMAT_OCTAL,	"octal", 	octalFormat },
    { RPMTD_FORMAT_HEX,		"hex", 		hexFormat },
    { RPMTD_FORMAT_DATE,	"date", 	dateFormat },
    { RPMTD_FORMAT_DAY,		"day", 		dayFormat },
    { RPMTD_FORMAT_SHESCAPE,	"shescape", 	shescapeFormat },
    { -1,			NULL, 		NULL }
};
