/** \ingroup header
 * \file lib/formats.c
 */

#include "system.h"

#include <inttypes.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmtd.h>
#include <rpm/rpmds.h>
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

/* forward declarations */
static const struct headerFormatFunc_s rpmHeaderFormats[];

void *rpmHeaderFormatFuncByName(const char *fmt);
void *rpmHeaderFormatFuncByValue(rpmtdFormats fmt);

/**
 * barebones string representation with no extra formatting
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * stringFormat(rpmtd td, char *formatPrefix)
{
    const char *str = NULL;
    char *val = NULL, *buf = NULL;

    switch (rpmtdClass(td)) {
	case RPM_NUMERIC_CLASS:
	    strcat(formatPrefix, PRIu64);
	    rasprintf(&val, formatPrefix, rpmtdGetNumber(td));
	    break;
	case RPM_STRING_CLASS:
	    str = rpmtdGetString(td);
	    strcat(formatPrefix, "s");
	    rasprintf(&val, formatPrefix, str);
	    break;
	case RPM_BINARY_CLASS:
	    buf = pgpHexStr(td->data, td->count);
	    strcat(formatPrefix, "s");
	    rasprintf(&val, formatPrefix, buf);
	    free(buf);
	    break;
	default:
	    val = xstrdup("(unknown type)");
	    break;
    }
    return val;
}

static char * numFormat(rpmtd td, char * formatPrefix, char *format)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	strcat(formatPrefix, format);
	rasprintf(&val, formatPrefix, rpmtdGetNumber(td));
    }

    return val;
}
/**
 * octalFormat.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * octalFormat(rpmtd td, char * formatPrefix)
{
    return numFormat(td, formatPrefix, "o");
}

/**
 * hexFormat.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * hexFormat(rpmtd td, char * formatPrefix)
{
    return numFormat(td, formatPrefix, "x");
}

/**
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * realDateFormat(rpmtd td, char * formatPrefix,
		const char * strftimeFormat)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	struct tm * tstruct;
	char buf[50];
	time_t dateint = rpmtdGetNumber(td);
	tstruct = localtime(&dateint);

	strcat(formatPrefix, "s");

	buf[0] = '\0';
	if (tstruct)
	    (void) strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	rasprintf(&val, formatPrefix, buf);
    }

    return val;
}

/**
 * Format a date.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * dateFormat(rpmtd td, char * formatPrefix)
{
    return realDateFormat(td, formatPrefix, _("%c"));
}

/**
 * Format a day.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * dayFormat(rpmtd td, char * formatPrefix)
{
    return realDateFormat(td, formatPrefix, _("%a %b %d %Y"));
}

/**
 * Return shell escape formatted data.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * shescapeFormat(rpmtd td, char * formatPrefix)
{
    char * result = NULL, * dst, * src;

    if (rpmtdClass(td) == RPM_NUMERIC_CLASS) {
	strcat(formatPrefix, PRIu64);
	rasprintf(&result, formatPrefix, rpmtdGetNumber(td));
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
 * @return		formatted string
 */
static char * triggertypeFormat(rpmtd td, char * formatPrefix)
{
    char * val;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	uint64_t item = rpmtdGetNumber(td);
	if (item & RPMSENSE_TRIGGERPREIN)
	    val = xstrdup("prein");
	else if (item & RPMSENSE_TRIGGERIN)
	    val = xstrdup("in");
	else if (item & RPMSENSE_TRIGGERUN)
	    val = xstrdup("un");
	else if (item & RPMSENSE_TRIGGERPOSTUN)
	    val = xstrdup("postun");
	else
	    val = xstrdup("");
    }
    return val;
}

/**
 * Identify type of dependency.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * deptypeFormat(rpmtd td, char * formatPrefix)
{
    char *val = NULL;
    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	ARGV_t sdeps = NULL;
	uint64_t item = rpmtdGetNumber(td);

	if (item & RPMSENSE_SCRIPT_PRE)
	    argvAdd(&sdeps, "pre");
	if (item & RPMSENSE_SCRIPT_POST)
	    argvAdd(&sdeps, "post");
	if (item & RPMSENSE_SCRIPT_PREUN)
	    argvAdd(&sdeps, "preun");
	if (item & RPMSENSE_SCRIPT_POSTUN)
	    argvAdd(&sdeps, "postun");
	if (item & RPMSENSE_SCRIPT_VERIFY)
	    argvAdd(&sdeps, "verify");
	if (item & RPMSENSE_INTERP)
	    argvAdd(&sdeps, "interp");
	if (item & RPMSENSE_RPMLIB)
	    argvAdd(&sdeps, "rpmlib");
	if ((item & RPMSENSE_FIND_REQUIRES) || (item & RPMSENSE_FIND_PROVIDES))
	    argvAdd(&sdeps, "auto");
	if (item & RPMSENSE_PREREQ)
	    argvAdd(&sdeps, "prereq");

	if (sdeps) {
	    val = argvJoin(sdeps, ",");
	} else {
	    val = xstrdup("manual");
	}

	argvFree(sdeps);
    }
    return val;
}

/**
 * Format file permissions for display.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * permsFormat(rpmtd td, char * formatPrefix)
{
    char * val = NULL;
    char * buf;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	strcat(formatPrefix, "s");
	buf = rpmPermsString(rpmtdGetNumber(td));
	rasprintf(&val, formatPrefix, buf);
	buf = _free(buf);
    }

    return val;
}

/**
 * Format file flags for display.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * fflagsFormat(rpmtd td, char * formatPrefix)
{
    char * val = NULL;
    char buf[15];

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	uint64_t anint = rpmtdGetNumber(td);
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

	strcat(formatPrefix, "s");
	rasprintf(&val, formatPrefix, buf);
    }

    return val;
}

/**
 * Wrap a pubkey in ascii armor for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * armorFormat(rpmtd td, char * formatPrefix)
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
    case RPM_INT64_TYPE:
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
 * @return		formatted string
 */
static char * base64Format(rpmtd td, char * formatPrefix)
{
    char * val = NULL;

    if (rpmtdType(td) != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	char * enc;
	if ((enc = b64encode(td->data, td->count, -1)) != NULL) {
	    strcat(formatPrefix, "s");
	    rasprintf(&val, formatPrefix, enc ? enc : "");
	    free(enc);
	}
    }

    return val;
}

/**
 * Wrap tag data in simple header xml markup.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * xmlFormat(rpmtd td, char * formatPrefix)
{
    const char *xtag = NULL;
    char *val = NULL;
    char *s = NULL;
    rpmtdFormats fmt = RPMTD_FORMAT_STRING;

    switch (rpmtdClass(td)) {
    case RPM_STRING_CLASS:
	xtag = "string";
	break;
    case RPM_BINARY_CLASS:
	fmt = RPMTD_FORMAT_BASE64;
	xtag = "base64";
	break;
    case RPM_NUMERIC_CLASS:
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
	val = rstrscat(NULL, "\t<", xtag, "/>", NULL);
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

	val = rstrscat(NULL, "\t<", xtag, ">", new_s, "</", xtag, ">", NULL);
	free(new_s);
    }
    free(s);

    strcat(formatPrefix, "s");
    return val;
}

/**
 * Display signature fingerprint and time.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * pgpsigFormat(rpmtd td, char * formatPrefix)
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

	    t = stpcpy(t, pgpValString(PGPVAL_PUBKEYALGO, sigp->pubkey_algo));
	    if (t + 5 >= val + nb)
		goto again;
	    *t++ = '/';
	    t = stpcpy(t, pgpValString(PGPVAL_HASHALGO, sigp->hash_algo));
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
 * @return		formatted string
 */
static char * depflagsFormat(rpmtd td, char * formatPrefix)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	uint64_t anint = rpmtdGetNumber(td);
	char buf[10];
	buf[0] = '\0';

	if (anint & RPMSENSE_LESS) 
	    strcat(buf, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(buf, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(buf, "=");

	strcat(formatPrefix, "s");
	rasprintf(&val, formatPrefix, buf);
    }

    return val;
}

/**
 * Return tag container array size.
 * @param td		tag data container
 * @param formatPrefix	sprintf format string
 * @return		formatted string
 */
static char * arraysizeFormat(rpmtd td, char * formatPrefix)
{
    char *val = NULL;
    strcat(formatPrefix, "u");
    rasprintf(&val, formatPrefix, rpmtdCount(td));
    return val;
}

static char * fstateFormat(rpmtd td, char * formatPrefix)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	const char * str;
	rpmfileState fstate = rpmtdGetNumber(td);
	switch (fstate) {
	case RPMFILE_STATE_NORMAL:
	    str = _("normal");
	    break;
	case RPMFILE_STATE_REPLACED:
	    str = _("replaced");
	    break;
	case RPMFILE_STATE_NOTINSTALLED:
	    str = _("not installed");
	    break;
	case RPMFILE_STATE_NETSHARED:
	    str = _("net shared");
	    break;
	case RPMFILE_STATE_WRONGCOLOR:
	    str = _("wrong color");
	    break;
	/* XXX headers should never have this value as file state */
	case RPMFILE_STATE_MISSING:
	    str = _("missing");
	    break;
	default:
	    str = _("(unknown)");
	    break;
	}
	
	strcat(formatPrefix, "s");
	rasprintf(&val, formatPrefix, str);
    }
    return val;
}

static char * vflagsFormat(rpmtd td, char * formatPrefix)
{
    char * val = NULL;
    char buf[15];

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	uint64_t vflags = rpmtdGetNumber(td);
	buf[0] = '\0';
	if (vflags & RPMVERIFY_FILEDIGEST)
	    strcat(buf, "5");
	if (vflags & RPMVERIFY_FILESIZE)
	    strcat(buf, "S");
	if (vflags & RPMVERIFY_LINKTO)
	    strcat(buf, "L");
	if (vflags & RPMVERIFY_MTIME)
	    strcat(buf, "T");
	if (vflags & RPMVERIFY_RDEV)
	    strcat(buf, "D");
	if (vflags & RPMVERIFY_USER)
	    strcat(buf, "U");
	if (vflags & RPMVERIFY_GROUP)
	    strcat(buf, "G");
	if (vflags & RPMVERIFY_MODE)
	    strcat(buf, "M");
	if (vflags & RPMVERIFY_CAPS)
	    strcat(buf, "P");

	strcat(formatPrefix, "s");
	rasprintf(&val, formatPrefix, buf);
    }

    return val;
}
void *rpmHeaderFormatFuncByName(const char *fmt)
{
    const struct headerFormatFunc_s * ext;
    void *func = NULL;

    for (ext = rpmHeaderFormats; ext->name != NULL; ext++) {
	if (rstreq(ext->name, fmt)) {
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

static const struct headerFormatFunc_s rpmHeaderFormats[] = {
    { RPMTD_FORMAT_STRING,	"string",	stringFormat },
    { RPMTD_FORMAT_ARMOR,	"armor",	armorFormat },
    { RPMTD_FORMAT_BASE64,	"base64",	base64Format },
    { RPMTD_FORMAT_PGPSIG,	"pgpsig",	pgpsigFormat },
    { RPMTD_FORMAT_DEPFLAGS,	"depflags",	depflagsFormat },
    { RPMTD_FORMAT_DEPTYPE,	"deptype",	deptypeFormat },
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
    { RPMTD_FORMAT_ARRAYSIZE,	"arraysize", 	arraysizeFormat },
    { RPMTD_FORMAT_FSTATE,	"fstate",	fstateFormat },
    { RPMTD_FORMAT_VFLAGS,	"vflags",	vflagsFormat },
    { -1,			NULL, 		NULL }
};
