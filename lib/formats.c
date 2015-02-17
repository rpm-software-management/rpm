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
#include <rpm/rpmmacro.h>
#include <rpm/rpmbase64.h>

#include "rpmio/digest.h"
#include "lib/manifest.h"
#include "lib/misc.h"
#include "lib/signature.h"

#include "debug.h"

/** \ingroup header
 * Define header tag output formats.
 */

struct headerFormatFunc_s {
    rpmtdFormats fmt;	/*!< Value of extension */
    const char *name;	/*!< Name of extension. */
    headerTagFormatFunction func;	/*!< Pointer to formatter function. */	
};

/**
 * barebones string representation with no extra formatting
 * @param td		tag data container
 * @return		formatted string
 */
static char * stringFormat(rpmtd td)
{
    char *val = NULL;

    switch (rpmtdClass(td)) {
	case RPM_NUMERIC_CLASS:
	    rasprintf(&val, "%" PRIu64, rpmtdGetNumber(td));
	    break;
	case RPM_STRING_CLASS:
	    val = xstrdup(rpmtdGetString(td));
	    break;
	case RPM_BINARY_CLASS:
	    val = pgpHexStr(td->data, td->count);
	    break;
	default:
	    val = xstrdup("(unknown type)");
	    break;
    }
    return val;
}

static char * numFormat(rpmtd td, const char *format)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	rasprintf(&val, format, rpmtdGetNumber(td));
    }

    return val;
}
/**
 * octalFormat.
 * @param td		tag data container
 * @return		formatted string
 */
static char * octalFormat(rpmtd td)
{
    return numFormat(td, "%o");
}

/**
 * hexFormat.
 * @param td		tag data container
 * @return		formatted string
 */
static char * hexFormat(rpmtd td)
{
    return numFormat(td, "%x");
}

/**
 * @param td			tag data container
 * @param strftimeFormat	format string
 * @return		formatted string
 */
static char * realDateFormat(rpmtd td, const char * strftimeFormat)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	struct tm * tstruct;
	char buf[50];
	time_t dateint = rpmtdGetNumber(td);
	tstruct = localtime(&dateint);

	/* XXX TODO: deal with non-fitting date string correctly */
	buf[0] = '\0';
	if (tstruct)
	    (void) strftime(buf, sizeof(buf) - 1, strftimeFormat, tstruct);
	val = xstrdup(buf);
    }

    return val;
}

/**
 * Format a date.
 * @param td		tag data container
 * @return		formatted string
 */
static char * dateFormat(rpmtd td)
{
    return realDateFormat(td, _("%c"));
}

/**
 * Format a day.
 * @param td		tag data container
 * @return		formatted string
 */
static char * dayFormat(rpmtd td)
{
    return realDateFormat(td, _("%a %b %d %Y"));
}

/**
 * Return shell escape formatted data.
 * @param td		tag data container
 * @return		formatted string
 */
static char * shescapeFormat(rpmtd td)
{
    char * result = NULL, * dst, * src;

    if (rpmtdClass(td) == RPM_NUMERIC_CLASS) {
	rasprintf(&result, "%" PRIu64, rpmtdGetNumber(td));
    } else {
	char *buf = xstrdup(rpmtdGetString(td));;

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
 * @return		formatted string
 */
static char * triggertypeFormat(rpmtd td)
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
 * @return		formatted string
 */
static char * deptypeFormat(rpmtd td)
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
	if (item & RPMSENSE_PRETRANS)
	    argvAdd(&sdeps, "pretrans");
	if (item & RPMSENSE_POSTTRANS)
	    argvAdd(&sdeps, "posttrans");
	if (item & RPMSENSE_CONFIG)
	    argvAdd(&sdeps, "config");
	if (item & RPMSENSE_MISSINGOK)
	    argvAdd(&sdeps, "missingok");

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
 * @return		formatted string
 */
static char * permsFormat(rpmtd td)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = rpmPermsString(rpmtdGetNumber(td));
    }

    return val;
}

/**
 * Format file flags for display.
 * @param td		tag data container
 * @return		formatted string
 */
static char * fflagsFormat(rpmtd td)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = rpmFFlagsString(rpmtdGetNumber(td), "");
    }

    return val;
}

/**
 * Wrap a pubkey in ascii armor for display.
 * @todo Permit selectable display formats (i.e. binary).
 * @param td		tag data container
 * @return		formatted string
 */
static char * armorFormat(rpmtd td)
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
	if (rpmBase64Decode(enc, (void **)&bs, &ns))
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
 * @return		formatted string
 */
static char * base64Format(rpmtd td)
{
    char * val = NULL;

    if (rpmtdType(td) != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	val = rpmBase64Encode(td->data, td->count, -1);
	if (val == NULL)
	    val = xstrdup("");
    }

    return val;
}

/**
 * Wrap tag data in simple header xml markup.
 * @param td		tag data container
 * @return		formatted string
 */
static char * xmlFormat(rpmtd td)
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

    return val;
}

/**
 * Display signature fingerprint and time.
 * @param td		tag data container
 * @return		formatted string
 */
static char * pgpsigFormat(rpmtd td)
{
    char * val = NULL;

    if (rpmtdType(td) != RPM_BIN_TYPE) {
	val = xstrdup(_("(not a blob)"));
    } else {
	pgpDigParams sigp = NULL;

	if (pgpPrtParams(td->data, td->count, PGPTAG_SIGNATURE, &sigp)) {
	    val = xstrdup(_("(not an OpenPGP signature)"));
	} else {
	    char dbuf[BUFSIZ];
	    char *keyid = pgpHexStr(sigp->signid, sizeof(sigp->signid));
	    unsigned int dateint = pgpGrab(sigp->time, sizeof(sigp->time));
	    time_t date = dateint;
	    struct tm * tms = localtime(&date);
	    unsigned int key_algo = pgpDigParamsAlgo(sigp, PGPVAL_PUBKEYALGO);
	    unsigned int hash_algo = pgpDigParamsAlgo(sigp, PGPVAL_HASHALGO);

	    if (!(tms && strftime(dbuf, sizeof(dbuf), "%c", tms) > 0)) {
		snprintf(dbuf, sizeof(dbuf),
			 _("Invalid date %u"), dateint);
		dbuf[sizeof(dbuf)-1] = '\0';
	    }

	    rasprintf(&val, "%s/%s, %s, Key ID %s",
			pgpValString(PGPVAL_PUBKEYALGO, key_algo),
			pgpValString(PGPVAL_HASHALGO, hash_algo),
			dbuf, keyid);

	    free(keyid);
	    pgpDigParamsFree(sigp);
	}
    }

    return val;
}

/**
 * Format dependency flags for display.
 * @param td		tag data container
 * @return		formatted string
 */
static char * depflagsFormat(rpmtd td)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	uint64_t anint = rpmtdGetNumber(td);
	val = xcalloc(4, 1);

	if (anint & RPMSENSE_LESS) 
	    strcat(val, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(val, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(val, "=");
    }

    return val;
}

/**
 * Return tag container array size.
 * @param td		tag data container
 * @return		formatted string
 */
static char * arraysizeFormat(rpmtd td)
{
    char *val = NULL;
    rasprintf(&val, "%u", rpmtdCount(td));
    return val;
}

static char * fstateFormat(rpmtd td)
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
	case RPMFILE_STATE_MISSING:
	    str = _("missing");
	    break;
	default:
	    str = _("(unknown)");
	    break;
	}
	
	val = xstrdup(str);
    }
    return val;
}

static char * verifyFlags(rpmtd td, const char *pad)
{
    char * val = NULL;

    if (rpmtdClass(td) != RPM_NUMERIC_CLASS) {
	val = xstrdup(_("(not a number)"));
    } else {
	val = rpmVerifyString(rpmtdGetNumber(td), pad);
    }
    return val;
}

static char * vflagsFormat(rpmtd td)
{
    return verifyFlags(td, "");
}

static char * fstatusFormat(rpmtd td)
{
    return verifyFlags(td, ".");
}

static char * expandFormat(rpmtd td)
{
    char *val = NULL;
    if (rpmtdClass(td) != RPM_STRING_CLASS) {
	val = xstrdup(_("(not a string)"));
    } else {
	val = rpmExpand(td->data, NULL);
    }
    return val;
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
    { RPMTD_FORMAT_EXPAND,	"expand",	expandFormat },
    { RPMTD_FORMAT_FSTATUS,	"fstatus",	fstatusFormat },
    { -1,			NULL, 		NULL }
};

headerTagFormatFunction rpmHeaderFormatFuncByName(const char *fmt)
{
    const struct headerFormatFunc_s * ext;
    headerTagFormatFunction func = NULL;

    for (ext = rpmHeaderFormats; ext->name != NULL; ext++) {
	if (rstreq(ext->name, fmt)) {
	    func = ext->func;
	    break;
	}
    }
    return func;
}

headerTagFormatFunction rpmHeaderFormatFuncByValue(rpmtdFormats fmt)
{
    const struct headerFormatFunc_s * ext;
    headerTagFormatFunction func = NULL;

    for (ext = rpmHeaderFormats; ext->name != NULL; ext++) {
	if (fmt == ext->fmt) {
	    func = ext->func;
	    break;
	}
    }
    return func;
}

