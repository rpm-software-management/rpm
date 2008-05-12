/** \ingroup header
 * \file lib/headerfmt.c
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmpgp.h>

#include "debug.h"

#define PARSER_BEGIN 	0
#define PARSER_IN_ARRAY 1
#define PARSER_IN_EXPR  2

/** \ingroup header
 */
typedef struct sprintfTag_s * sprintfTag;
struct sprintfTag_s {
    headerTagFormatFunction fmt;
    headerTagTagFunction ext;   /*!< NULL if tag element is invalid */
    int extNum;
    rpmTag tag;
    int justOne;
    int arrayCount;
    char * format;
    char * type;
    int pad;
};

/** \ingroup header
 * Extension cache.
 */
typedef struct rpmec_s * rpmec;
struct rpmec_s {
    rpmTagType type;
    rpm_count_t count;
    int avail;
    int freeit;
    rpm_data_t data;
};

/** \ingroup header
 */
typedef struct sprintfToken_s * sprintfToken;
struct sprintfToken_s {
    enum {
	PTOK_NONE = 0,
	PTOK_TAG,
	PTOK_ARRAY,
	PTOK_STRING,
	PTOK_COND
    } type;
    union {
	struct sprintfTag_s tag;	/*!< PTOK_TAG */
	struct {
	    sprintfToken format;
	    int i;
	    int numTokens;
	} array;			/*!< PTOK_ARRAY */
	struct {
	    char * string;
	    int len;
	} string;			/*!< PTOK_STRING */
	struct {
	    sprintfToken ifFormat;
	    int numIfTokens;
	    sprintfToken elseFormat;
	    int numElseTokens;
	    struct sprintfTag_s tag;
	} cond;				/*!< PTOK_COND */
    } u;
};

/**
 */
typedef struct headerSprintfArgs_s {
    Header h;
    char * fmt;
    headerTagTableEntry tags;
    headerSprintfExtension exts;
    const char * errmsg;
    rpmec ec;
    sprintfToken format;
    HeaderIterator hi;
    char * val;
    size_t vallen;
    size_t alloced;
    int numTokens;
    int i;
} * headerSprintfArgs;


static char escapedChar(const char ch)	
{
    switch (ch) {
    case 'a': 	return '\a';
    case 'b': 	return '\b';
    case 'f': 	return '\f';
    case 'n': 	return '\n';
    case 'r': 	return '\r';
    case 't': 	return '\t';
    case 'v': 	return '\v';
    default:	return ch;
    }
}

/**
 * Destroy headerSprintf format array.
 * @param format	sprintf format array
 * @param num		number of elements
 * @return		NULL always
 */
static sprintfToken
freeFormat( sprintfToken format, int num)
{
    int i;

    if (format == NULL) return NULL;

    for (i = 0; i < num; i++) {
	switch (format[i].type) {
	case PTOK_ARRAY:
	    format[i].u.array.format =
		freeFormat(format[i].u.array.format,
			format[i].u.array.numTokens);
	    break;
	case PTOK_COND:
	    format[i].u.cond.ifFormat =
		freeFormat(format[i].u.cond.ifFormat, 
			format[i].u.cond.numIfTokens);
	    format[i].u.cond.elseFormat =
		freeFormat(format[i].u.cond.elseFormat, 
			format[i].u.cond.numElseTokens);
	    break;
	case PTOK_NONE:
	case PTOK_TAG:
	case PTOK_STRING:
	default:
	    break;
	}
    }
    format = _free(format);
    return NULL;
}

/**
 * Initialize an hsa iteration.
 * @param hsa		headerSprintf args
 */
static void hsaInit(headerSprintfArgs hsa)
{
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    hsa->i = 0;
    if (tag != NULL && tag->tag == -2)
	hsa->hi = headerInitIterator(hsa->h);
}

/**
 * Return next hsa iteration item.
 * @param hsa		headerSprintf args
 * @return		next sprintfToken (or NULL)
 */
static sprintfToken hsaNext(headerSprintfArgs hsa)
{
    sprintfToken fmt = NULL;
    sprintfTag tag =
	(hsa->format->type == PTOK_TAG
	    ? &hsa->format->u.tag :
	(hsa->format->type == PTOK_ARRAY
	    ? &hsa->format->u.array.format->u.tag :
	NULL));

    if (hsa->i >= 0 && hsa->i < hsa->numTokens) {
	fmt = hsa->format + hsa->i;
	if (hsa->hi == NULL) {
	    hsa->i++;
	} else {
	    rpmTag tagno;
	    rpmTagType type;
	    rpm_count_t count;

	    if (!headerNextIterator(hsa->hi, &tagno, &type, NULL, &count))
		fmt = NULL;
	    tag->tag = tagno;
	}
    }

    return fmt;
}

/**
 * Finish an hsa iteration.
 * @param hsa		headerSprintf args
 */
static void hsaFini(headerSprintfArgs hsa)
{
    hsa->hi = headerFreeIterator(hsa->hi);
    hsa->i = 0;
}

/**
 * Reserve sufficient buffer space for next output value.
 * @param hsa		headerSprintf args
 * @param need		no. of bytes to reserve
 * @return		pointer to reserved space
 */
static char * hsaReserve(headerSprintfArgs hsa, size_t need)
{
    if ((hsa->vallen + need) >= hsa->alloced) {
	if (hsa->alloced <= need)
	    hsa->alloced += need;
	hsa->alloced <<= 1;
	hsa->val = xrealloc(hsa->val, hsa->alloced+1);	
    }
    return hsa->val + hsa->vallen;
}

/**
 * Return tag name from value.
 * @todo bsearch on sorted value table.
 * @param tbl		tag table
 * @param val		tag value to find
 * @return		tag name, NULL on not found
 */
static const char * myTagName(headerTagTableEntry tbl, int val)
{
    static char name[128];
    const char * s;
    char *t;

    for (; tbl->name != NULL; tbl++) {
	if (tbl->val == val)
	    break;
    }
    if ((s = tbl->name) == NULL)
	return NULL;
    s += sizeof("RPMTAG_") - 1;
    t = name;
    *t++ = *s++;
    while (*s != '\0')
	*t++ = rtolower(*s++);
    *t = '\0';
    return name;
}

/**
 * Return tag value from name.
 * @todo bsearch on sorted name table.
 * @param tbl		tag table
 * @param name		tag name to find
 * @return		tag value, 0 on not found
 */
static int myTagValue(headerTagTableEntry tbl, const char * name)
{
    for (; tbl->name != NULL; tbl++) {
	if (!rstrcasecmp(tbl->name + sizeof("RPMTAG"), name))
	    return tbl->val;
    }
    return 0;
}

/**
 * Search extensions and tags for a name.
 * @param hsa		headerSprintf args
 * @param token		parsed fields
 * @param name		name to find
 * @return		0 on success, 1 on not found
 */
static int findTag(headerSprintfArgs hsa, sprintfToken token, const char * name)
{
    const char *tagname = name;
    headerSprintfExtension ext;
    sprintfTag stag = (token->type == PTOK_COND
	? &token->u.cond.tag : &token->u.tag);

    stag->fmt = NULL;
    stag->ext = NULL;
    stag->extNum = 0;
    stag->tag = -1;

    if (!strcmp(tagname, "*")) {
	stag->tag = -2;
	goto bingo;
    }

    if (strncmp("RPMTAG_", tagname, sizeof("RPMTAG_")-1) == 0) {
	tagname += sizeof("RPMTAG");
    }

    /* Search extensions for specific tag override. */
    for (ext = hsa->exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_TAG)
	    continue;
	if (!rstrcasecmp(ext->name + sizeof("RPMTAG"), tagname)) {
	    stag->ext = ext->u.tagFunction;
	    stag->extNum = ext - hsa->exts;
	    goto bingo;
	}
    }

    /* Search tag names. */
    stag->tag = myTagValue(hsa->tags, tagname);
    if (stag->tag != 0)
	goto bingo;

    return 1;

bingo:
    /* Search extensions for specific format. */
    if (stag->type != NULL)
    for (ext = hsa->exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	    ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ext->name == NULL || ext->type != HEADER_EXT_FORMAT)
	    continue;
	if (!strcmp(ext->name, stag->type)) {
	    stag->fmt = ext->u.formatFunction;
	    break;
	}
    }
    return 0;
}

/* forward ref */
/**
 * Parse an expression.
 * @param hsa		headerSprintf args
 * @param token		token
 * @param str		string
 * @param[out] *endPtr
 * @return		0 on success
 */
static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str,char ** endPtr);

/**
 * Parse a headerSprintf term.
 * @param hsa		headerSprintf args
 * @param str
 * @retval *formatPtr
 * @retval *numTokensPtr
 * @retval *endPtr
 * @param state
 * @return		0 on success
 */
static int parseFormat(headerSprintfArgs hsa, char * str,
	sprintfToken * formatPtr,int * numTokensPtr,
	char ** endPtr, int state)
{
    char * chptr, * start, * next, * dst;
    sprintfToken format;
    sprintfToken token;
    int numTokens;
    int i;
    int done = 0;

    /* upper limit on number of individual formats */
    numTokens = 0;
    if (str != NULL)
    for (chptr = str; *chptr != '\0'; chptr++)
	if (*chptr == '%') numTokens++;
    numTokens = numTokens * 2 + 1;

    format = xcalloc(numTokens, sizeof(*format));
    if (endPtr) *endPtr = NULL;

    /* LCL: can't detect done termination */
    dst = start = str;
    numTokens = 0;
    token = NULL;
    if (start != NULL)
    while (*start != '\0') {
	switch (*start) {
	case '%':
	    /* handle %% */
	    if (*(start + 1) == '%') {
		if (token == NULL || token->type != PTOK_STRING) {
		    token = format + numTokens++;
		    token->type = PTOK_STRING;
		    dst = token->u.string.string = start;
		}
		start++;
		*dst++ = *start++;
		break;
	    } 

	    token = format + numTokens++;
	    *dst++ = '\0';
	    start++;

	    if (*start == '|') {
		char * newEnd;

		start++;
		if (parseExpression(hsa, token, start, &newEnd))
		{
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		start = newEnd;
		break;
	    }

	    token->u.tag.format = start;
	    token->u.tag.pad = 0;
	    token->u.tag.justOne = 0;
	    token->u.tag.arrayCount = 0;

	    chptr = start;
	    while (*chptr && *chptr != '{' && *chptr != '%') chptr++;
	    if (!*chptr || *chptr == '%') {
		hsa->errmsg = _("missing { after %");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    *chptr++ = '\0';

	    while (start < chptr) {
		if (risdigit(*start)) {
		    i = strtoul(start, &start, 10);
		    token->u.tag.pad += i;
		    start = chptr;
		    break;
		} else {
		    start++;
		}
	    }

	    if (*start == '=') {
		token->u.tag.justOne = 1;
		start++;
	    } else if (*start == '#') {
		token->u.tag.justOne = 1;
		token->u.tag.arrayCount = 1;
		start++;
	    }

	    dst = next = start;
	    while (*next && *next != '}') next++;
	    if (!*next) {
		hsa->errmsg = _("missing } after %{");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *next++ = '\0';

	    chptr = start;
	    while (*chptr && *chptr != ':') chptr++;

	    if (*chptr != '\0') {
		*chptr++ = '\0';
		if (!*chptr) {
		    hsa->errmsg = _("empty tag format");
		    format = freeFormat(format, numTokens);
		    return 1;
		}
		token->u.tag.type = chptr;
	    } else {
		token->u.tag.type = NULL;
	    }
	    
	    if (!*start) {
		hsa->errmsg = _("empty tag name");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    i = 0;
	    token->type = PTOK_TAG;

	    if (findTag(hsa, token, start)) {
		hsa->errmsg = _("unknown tag");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    start = next;
	    break;

	case '[':
	    *dst++ = '\0';
	    *start++ = '\0';
	    token = format + numTokens++;

	    if (parseFormat(hsa, start,
			    &token->u.array.format,
			    &token->u.array.numTokens,
			    &start, PARSER_IN_ARRAY))
	    {
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    if (!start) {
		hsa->errmsg = _("] expected at end of array");
		format = freeFormat(format, numTokens);
		return 1;
	    }

	    dst = start;

	    token->type = PTOK_ARRAY;

	    break;

	case ']':
	    if (state != PARSER_IN_ARRAY) {
		hsa->errmsg = _("unexpected ]");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    break;

	case '}':
	    if (state != PARSER_IN_EXPR) {
		hsa->errmsg = _("unexpected }");
		format = freeFormat(format, numTokens);
		return 1;
	    }
	    *start++ = '\0';
	    if (endPtr) *endPtr = start;
	    done = 1;
	    break;

	default:
	    if (token == NULL || token->type != PTOK_STRING) {
		token = format + numTokens++;
		token->type = PTOK_STRING;
		dst = token->u.string.string = start;
	    }

	    if (*start == '\\') {
		start++;
		*dst++ = escapedChar(*start++);
	    } else {
		*dst++ = *start++;
	    }
	    break;
	}
	if (done)
	    break;
    }

    if (dst != NULL)
        *dst = '\0';

    for (i = 0; i < numTokens; i++) {
	token = format + i;
	if (token->type == PTOK_STRING)
	    token->u.string.len = strlen(token->u.string.string);
    }

    *numTokensPtr = numTokens;
    *formatPtr = format;

    return 0;
}

static int parseExpression(headerSprintfArgs hsa, sprintfToken token,
		char * str, char ** endPtr)
{
    char * chptr;
    char * end;

    hsa->errmsg = NULL;
    chptr = str;
    while (*chptr && *chptr != '?') chptr++;

    if (*chptr != '?') {
	hsa->errmsg = _("? expected in expression");
	return 1;
    }

    *chptr++ = '\0';;

    if (*chptr != '{') {
	hsa->errmsg = _("{ expected after ? in expression");
	return 1;
    }

    chptr++;

    if (parseFormat(hsa, chptr, &token->u.cond.ifFormat, 
		    &token->u.cond.numIfTokens, &end, PARSER_IN_EXPR)) 
	return 1;

    /* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{%}:{NAME}|\n'"*/
    if (!(end && *end)) {
	hsa->errmsg = _("} expected in expression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    chptr = end;
    if (*chptr != ':' && *chptr != '|') {
	hsa->errmsg = _(": expected following ? subexpression");
	token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	return 1;
    }

    if (*chptr == '|') {
	if (parseFormat(hsa, NULL, &token->u.cond.elseFormat, 
		&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR))
	{
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}
    } else {
	chptr++;

	if (*chptr != '{') {
	    hsa->errmsg = _("{ expected after : in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr++;

	if (parseFormat(hsa, chptr, &token->u.cond.elseFormat, 
			&token->u.cond.numElseTokens, &end, PARSER_IN_EXPR)) 
	    return 1;

	/* XXX fix segfault on "rpm -q rpm --qf='%|NAME?{a}:{%}|{NAME}\n'" */
	if (!(end && *end)) {
	    hsa->errmsg = _("} expected in expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    return 1;
	}

	chptr = end;
	if (*chptr != '|') {
	    hsa->errmsg = _("| expected at end of expression");
	    token->u.cond.ifFormat =
		freeFormat(token->u.cond.ifFormat, token->u.cond.numIfTokens);
	    token->u.cond.elseFormat =
		freeFormat(token->u.cond.elseFormat, token->u.cond.numElseTokens);
	    return 1;
	}
    }
	
    chptr++;

    *endPtr = chptr;

    token->type = PTOK_COND;

    (void) findTag(hsa, token, str);

    return 0;
}

/**
 * Call a header extension only once, saving results.
 * @param hsa		headerSprintf args
 * @param fn
 * @retval *typeptr
 * @retval *data
 * @retval *countptr
 * @retval ec		extension cache
 * @return		0 on success, 1 on failure
 */
static int getExtension(headerSprintfArgs hsa, headerTagTagFunction fn,
		rpmTagType * typeptr,
		rpm_data_t * data,
		rpm_count_t * countptr,
		rpmec ec)
{
    if (!ec->avail) {
	if (fn(hsa->h, &ec->type, &ec->data, &ec->count, &ec->freeit))
	    return 1;
	ec->avail = 1;
    }

    if (typeptr) *typeptr = ec->type;
    if (data) *data = ec->data;
    if (countptr) *countptr = ec->count;

    return 0;
}

/**
 * formatValue
 * @param hsa		headerSprintf args
 * @param tag
 * @param element
 * @return		end of formatted string (NULL on error)
 */
static char * formatValue(headerSprintfArgs hsa, sprintfTag tag, int element)
{
    char * val = NULL;
    size_t need = 0;
    char * t, * te;
    char buf[20];
    rpmTagType type;
    rpm_count_t count;
    rpm_data_t data;
    unsigned int intVal;
    const char ** strarray;
    int datafree = 0;
    int countBuf;

    memset(buf, 0, sizeof(buf));
    if (tag->ext) {
	if (getExtension(hsa, tag->ext, &type, &data, &count, hsa->ec + tag->extNum))
	{
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";
	}
    } else {
	if (!headerGetEntry(hsa->h, tag->tag, &type, &data, &count)) {
	    count = 1;
	    type = RPM_STRING_TYPE;	
	    data = "(none)";
	}

	/* XXX this test is unnecessary, array sizes are checked */
	switch (type) {
	default:
	    if (element >= count) {
		data = headerFreeData(data, type);

		hsa->errmsg = _("(index out of range)");
		return NULL;
	    }
	    break;
	case RPM_BIN_TYPE:
	case RPM_STRING_TYPE:
	    break;
	}
	datafree = 1;
    }

    if (tag->arrayCount) {
	if (datafree)
	    data = headerFreeData(data, type);

	countBuf = count;
	data = &countBuf;
	count = 1;
	type = RPM_INT32_TYPE;
    }

    (void) stpcpy( stpcpy(buf, "%"), tag->format);

    if (data)
    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
	strarray = (const char **)data;

	if (tag->fmt)
	    val = tag->fmt(RPM_STRING_TYPE, strarray[element], buf, tag->pad, 
			   (rpm_count_t) element);

	if (val) {
	    need = strlen(val);
	} else {
	    need = strlen(strarray[element]) + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "s");
	    sprintf(val, buf, strarray[element]);
	}

	break;

    case RPM_STRING_TYPE:
	if (tag->fmt)
	    val = tag->fmt(RPM_STRING_TYPE, data, buf, tag->pad,  0);

	if (val) {
	    need = strlen(val);
	} else {
	    need = strlen(data) + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "s");
	    sprintf(val, buf, data);
	}
	break;

    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
	switch (type) {
	case RPM_CHAR_TYPE:	
	case RPM_INT8_TYPE:
	    intVal = *(((int8_t *) data) + element);
	    break;
	case RPM_INT16_TYPE:
	    intVal = *(((uint16_t *) data) + element);
	    break;
	default:		/* keep -Wall quiet */
	case RPM_INT32_TYPE:
	    intVal = *(((int32_t *) data) + element);
	    break;
	}

	if (tag->fmt)
	    val = tag->fmt(RPM_INT32_TYPE, &intVal, buf, tag->pad, 
			   (rpm_count_t) element);

	if (val) {
	    need = strlen(val);
	} else {
	    need = 10 + tag->pad + 20;
	    val = xmalloc(need+1);
	    strcat(buf, "d");
	    sprintf(val, buf, intVal);
	}
	break;

    case RPM_BIN_TYPE:
	/* XXX HACK ALERT: element field abused as no. bytes of binary data. */
	if (tag->fmt)
	    val = tag->fmt(RPM_BIN_TYPE, data, buf, tag->pad, (rpm_count_t) count);

	if (val) {
	    need = strlen(val);
	} else {
	    val = pgpHexStr(data, count);
	    need = strlen(val) + tag->pad;
	}
	break;

    default:
	need = sizeof("(unknown type)") - 1;
	val = xstrdup("(unknown type)");
	break;
    }

    if (datafree)
	data = headerFreeData(data, type);

    if (val && need > 0) {
	t = hsaReserve(hsa, need);
	te = stpcpy(t, val);
	hsa->vallen += (te - t);
	val = _free(val);
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Format a single headerSprintf item.
 * @param hsa		headerSprintf args
 * @param token
 * @param element
 * @return		end of formatted string (NULL on error)
 */
static char * singleSprintf(headerSprintfArgs hsa, sprintfToken token,
		int element)
{
    char * t, * te;
    int i, j, found;
    rpmTagType type;
    rpm_count_t count, numElements;
    sprintfToken spft;
    int condNumFormats;
    size_t need;

    /* we assume the token and header have been validated already! */

    switch (token->type) {
    case PTOK_NONE:
	break;

    case PTOK_STRING:
	need = token->u.string.len;
	if (need == 0) break;
	t = hsaReserve(hsa, need);
	te = stpcpy(t, token->u.string.string);
	hsa->vallen += (te - t);
	break;

    case PTOK_TAG:
	t = hsa->val + hsa->vallen;
	te = formatValue(hsa, &token->u.tag,
			(token->u.tag.justOne ? 0 : element));
	if (te == NULL)
	    return NULL;
	break;

    case PTOK_COND:
	if (token->u.cond.tag.ext || headerIsEntry(hsa->h, token->u.cond.tag.tag)) {
	    spft = token->u.cond.ifFormat;
	    condNumFormats = token->u.cond.numIfTokens;
	} else {
	    spft = token->u.cond.elseFormat;
	    condNumFormats = token->u.cond.numElseTokens;
	}

	need = condNumFormats * 20;
	if (spft == NULL || need == 0) break;

	t = hsaReserve(hsa, need);
	for (i = 0; i < condNumFormats; i++, spft++) {
	    te = singleSprintf(hsa, spft, element);
	    if (te == NULL)
		return NULL;
	}
	break;

    case PTOK_ARRAY:
	numElements = 0;
	found = 0;
	spft = token->u.array.format;
	for (i = 0; i < token->u.array.numTokens; i++, spft++)
	{
	    if (spft->type != PTOK_TAG ||
		spft->u.tag.arrayCount ||
		spft->u.tag.justOne) continue;

	    if (spft->u.tag.ext) {
		if (getExtension(hsa, spft->u.tag.ext, &type, NULL, &count, 
				 hsa->ec + spft->u.tag.extNum))
		     continue;
	    } else {
		if (!headerGetEntry(hsa->h, spft->u.tag.tag, &type, NULL, &count))
		    continue;
	    } 

	    found = 1;

	    if (type == RPM_BIN_TYPE)
		count = 1;	/* XXX count abused as no. of bytes. */

	    if (numElements > 1 && count != numElements)
	    switch (type) {
	    default:
		hsa->errmsg =
			_("array iterator used with different sized arrays");
		return NULL;
		break;
	    case RPM_BIN_TYPE:
	    case RPM_STRING_TYPE:
		break;
	    }
	    if (count > numElements)
		numElements = count;
	}

	if (! found) {
	    need = sizeof("(none)") - 1;
	    t = hsaReserve(hsa, need);
	    te = stpcpy(t, "(none)");
	    hsa->vallen += (te - t);
	} else {
	    int isxml;

	    need = numElements * token->u.array.numTokens * 10;
	    if (need == 0) break;

	    spft = token->u.array.format;
	    isxml = (spft->type == PTOK_TAG && spft->u.tag.type != NULL &&
		!strcmp(spft->u.tag.type, "xml"));

	    if (isxml) {
		const char * tagN = myTagName(hsa->tags, spft->u.tag.tag);

		need = sizeof("  <rpmTag name=\"\">\n") - 1;
		if (tagN != NULL)
		    need += strlen(tagN);
		t = hsaReserve(hsa, need);
		te = stpcpy(t, "  <rpmTag name=\"");
		if (tagN != NULL)
		    te = stpcpy(te, tagN);
		te = stpcpy(te, "\">\n");
		hsa->vallen += (te - t);
	    }

	    t = hsaReserve(hsa, need);
	    for (j = 0; j < numElements; j++) {
		spft = token->u.array.format;
		for (i = 0; i < token->u.array.numTokens; i++, spft++) {
		    te = singleSprintf(hsa, spft, j);
		    if (te == NULL)
			return NULL;
		}
	    }

	    if (isxml) {
		need = sizeof("  </rpmTag>\n") - 1;
		t = hsaReserve(hsa, need);
		te = stpcpy(t, "  </rpmTag>\n");
		hsa->vallen += (te - t);
	    }

	}
	break;
    }

    return (hsa->val + hsa->vallen);
}

/**
 * Create an extension cache.
 * @param exts		headerSprintf extensions
 * @return		new extension cache
 */
static rpmec
rpmecNew(const headerSprintfExtension exts)
{
    headerSprintfExtension ext;
    rpmec ec;
    int i = 0;

    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	i++;
    }

    ec = xcalloc(i, sizeof(*ec));
    return ec;
}

/**
 * Destroy an extension cache.
 * @param exts		headerSprintf extensions
 * @param ec		extension cache
 * @return		NULL always
 */
static rpmec
rpmecFree(const headerSprintfExtension exts, rpmec ec)
{
    headerSprintfExtension ext;
    int i = 0;

    for (ext = exts; ext != NULL && ext->type != HEADER_EXT_LAST;
	ext = (ext->type == HEADER_EXT_MORE ? ext->u.more : ext+1))
    {
	if (ec[i].freeit) ec[i].data = _free(ec[i].data);
	i++;
    }

    ec = _free(ec);
    return NULL;
}

static char * intHeaderSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     errmsg_t * errmsg)
{
    struct headerSprintfArgs_s hsa;
    sprintfToken nextfmt;
    sprintfTag tag;
    char * t, * te;
    int isxml;
    size_t need;
 
    memset(&hsa, 0, sizeof(hsa));
    hsa.h = headerLink(h);
    hsa.fmt = xstrdup(fmt);
    hsa.exts = (headerSprintfExtension) extensions;
    hsa.tags = (headerTagTableEntry) tbltags;
    hsa.errmsg = NULL;

    if (parseFormat(&hsa, hsa.fmt, &hsa.format, &hsa.numTokens, NULL, PARSER_BEGIN))
	goto exit;

    hsa.ec = rpmecNew(hsa.exts);
    hsa.val = xstrdup("");

    tag =
	(hsa.format->type == PTOK_TAG
	    ? &hsa.format->u.tag :
	(hsa.format->type == PTOK_ARRAY
	    ? &hsa.format->u.array.format->u.tag :
	NULL));
    isxml = (tag != NULL && tag->tag == -2 && tag->type != NULL && !strcmp(tag->type, "xml"));

    if (isxml) {
	need = sizeof("<rpmHeader>\n") - 1;
	t = hsaReserve(&hsa, need);
	te = stpcpy(t, "<rpmHeader>\n");
	hsa.vallen += (te - t);
    }

    hsaInit(&hsa);
    while ((nextfmt = hsaNext(&hsa)) != NULL) {
	te = singleSprintf(&hsa, nextfmt, 0);
	if (te == NULL) {
	    hsa.val = _free(hsa.val);
	    break;
	}
    }
    hsaFini(&hsa);

    if (isxml) {
	need = sizeof("</rpmHeader>\n") - 1;
	t = hsaReserve(&hsa, need);
	te = stpcpy(t, "</rpmHeader>\n");
	hsa.vallen += (te - t);
    }

    if (hsa.val != NULL && hsa.vallen < hsa.alloced)
	hsa.val = xrealloc(hsa.val, hsa.vallen+1);	

    hsa.ec = rpmecFree(hsa.exts, hsa.ec);
    hsa.format = freeFormat(hsa.format, hsa.numTokens);

exit:
    if (errmsg)
	*errmsg = hsa.errmsg;
    hsa.h = headerFree(hsa.h);
    hsa.fmt = _free(hsa.fmt);
    return hsa.val;
}

char * headerFormat(Header h, const char * fmt, errmsg_t * errmsg) 
{
    return intHeaderSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, errmsg);
}

char * headerSprintf(Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tbltags,
		     const struct headerSprintfExtension_s * extensions,
		     errmsg_t * errmsg)
{
    return headerFormat(h, fmt, errmsg);
}

