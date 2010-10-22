/**
 * \file lib/tagname.c
 */

#include "system.h"

#include <rpm/header.h>
#include <rpm/rpmstring.h>
#include "debug.h"

/** \ingroup header
 * Associate tag names with numeric values.
 */
typedef const struct headerTagTableEntry_s * headerTagTableEntry;
struct headerTagTableEntry_s {
    const char * name;		/*!< Tag name. */
    const char * shortname;	/*!< "Human readable" short name. */
    rpmTagVal val;		/*!< Tag numeric value. */
    rpmTagType type;		/*!< Tag type. */
    rpmTagReturnType retype;	/*!< Tag return type. */
    int extension;		/*!< Extension or "real" tag */
};

#include "lib/tagtbl.C"

static const int rpmTagTableSize = sizeof(rpmTagTable) / sizeof(rpmTagTable[0]) - 1;

/**
 */
typedef struct headerTagIndices_s * headerTagIndices;

struct headerTagIndices_s {
    int (*loadIndex) (headerTagTableEntry ** ipp, int * np,
                int (*cmp) (const void * avp, const void * bvp));
                                        /*!< load sorted tag index. */
    headerTagTableEntry * byName;	/*!< header tags sorted by name. */
    int byNameSize;			/*!< no. of entries. */
    int (*byNameCmp) (const void * avp, const void * bvp);				/*!< compare entries by name. */
    rpmTagVal (*tagValue) (const char * name);	/* return value from name. */
    headerTagTableEntry * byValue;	/*!< header tags sorted by value. */
    int byValueSize;			/*!< no. of entries. */
    int (*byValueCmp) (const void * avp, const void * bvp);				/*!< compare entries by value. */
    const char * (*tagName) (rpmTagVal value);	/* Return name from value. */
    rpmTagType (*tagType) (rpmTagVal value);	/* Return type from value. */
};

/**
 * Compare tag table entries by name.
 * @param *avp		tag table entry a
 * @param *bvp		tag table entry b
 * @return		comparison
 */
static int tagCmpName(const void * avp, const void * bvp)
{
    headerTagTableEntry a = *(const headerTagTableEntry *) avp;
    headerTagTableEntry b = *(const headerTagTableEntry *) bvp;
    return strcmp(a->name, b->name);
}

/**
 * Compare tag table entries by value.
 * @param *avp		tag table entry a
 * @param *bvp		tag table entry b
 * @return		comparison
 */
static int tagCmpValue(const void * avp, const void * bvp)
{
    headerTagTableEntry a = *(const headerTagTableEntry *) avp;
    headerTagTableEntry b = *(const headerTagTableEntry *) bvp;
    int ret = (a->val - b->val);
    /* Make sure that sort is stable, longest name first. */
    if (ret == 0)
	ret = (strlen(b->name) - strlen(a->name));
    return ret;
}

/**
 * Load/sort a tag index.
 * @retval *ipp		tag index
 * @retval *np		no. of tags
 * @param cmp		sort compare routine
 * @return		0 always
 */
static int tagLoadIndex(headerTagTableEntry ** ipp, int * np,
		int (*cmp) (const void * avp, const void * bvp))
{
    headerTagTableEntry tte, *ip;
    int n = 0;

    ip = xcalloc(rpmTagTableSize, sizeof(*ip));
    n = 0;
    for (tte = (headerTagTableEntry)rpmTagTable; tte->name != NULL; tte++) {
	ip[n] = tte;
	n++;
    }
assert(n == rpmTagTableSize);

    if (n > 1)
	qsort(ip, n, sizeof(*ip), cmp);
    *ipp = ip;
    *np = n;
    return 0;
}


/* forward refs */
static const char * _tagName(rpmTagVal tag);
static rpmTagType _tagType(rpmTagVal tag);
static rpmTagVal _tagValue(const char * tagstr);

static struct headerTagIndices_s _rpmTags = {
    tagLoadIndex,
    NULL, 0, tagCmpName, _tagValue,
    NULL, 0, tagCmpValue, _tagName, _tagType,
};

static headerTagIndices const rpmTags = &_rpmTags;

static const char * _tagName(rpmTagVal tag)
{
    const char *name = "(unknown)";
    const struct headerTagTableEntry_s *t;
    int comparison, i, l, u;
    int xx;

    if (_rpmTags.byValue == NULL)
	xx = tagLoadIndex(&_rpmTags.byValue, &_rpmTags.byValueSize, tagCmpValue);

    switch (tag) {
    case RPMDBI_PACKAGES:
	name = "Packages";
	break;
    /* XXX make sure rpmdb indices are identically named. */
    case RPMTAG_CONFLICTS:
	name = "Conflictname";
	break;
    case RPMTAG_HDRID:
	name = "Sha1header";
	break;

    default:
	if (_rpmTags.byValue == NULL)
	    break;
	l = 0;
	u = _rpmTags.byValueSize;
	while (l < u) {
	    i = (l + u) / 2;
	    t = _rpmTags.byValue[i];
	
	    comparison = (tag - t->val);

	    if (comparison < 0)
		u = i;
	    else if (comparison > 0)
		l = i + 1;
	    else {
		/* Make sure that the bsearch retrieve is stable. */
		while (i > 0 && tag == _rpmTags.byValue[i-1]->val) {
		    i--;
		}
		t = _rpmTags.byValue[i];
		if (t->shortname != NULL)
		    name = t->shortname;
		break;
	    }
	}
	break;
    }
    return name;
}

static rpmTagType _tagType(rpmTagVal tag)
{
    const struct headerTagTableEntry_s *t;
    int comparison, i, l, u;
    int xx;

    if (_rpmTags.byValue == NULL)
	xx = tagLoadIndex(&_rpmTags.byValue, &_rpmTags.byValueSize, tagCmpValue);
    if (_rpmTags.byValue) {
	l = 0;
	u = _rpmTags.byValueSize;
	while (l < u) {
	    i = (l + u) / 2;
	    t = _rpmTags.byValue[i];
	
	    comparison = (tag - t->val);

	    if (comparison < 0)
		u = i;
	    else if (comparison > 0)
		l = i + 1;
	    else {
		/* Make sure that the bsearch retrieve is stable. */
		while (i > 0 && t->val == _rpmTags.byValue[i-1]->val) {
		    i--;
		}
		t = _rpmTags.byValue[i];
		/* XXX this is dumb */
		return (rpmTagType)(t->type | t->retype);
	    }
	}
    }
    return RPM_NULL_TYPE;
}

static rpmTagVal _tagValue(const char * tagstr)
{
    const struct headerTagTableEntry_s *t;
    int comparison, i, l, u;
    int xx;

    if (!rstrcasecmp(tagstr, "Packages"))
	return RPMDBI_PACKAGES;

    if (_rpmTags.byName == NULL)
	xx = tagLoadIndex(&_rpmTags.byName, &_rpmTags.byNameSize, tagCmpName);
    if (_rpmTags.byName == NULL)
	return RPMTAG_NOT_FOUND;

    l = 0;
    u = _rpmTags.byNameSize;
    while (l < u) {
	i = (l + u) / 2;
	t = _rpmTags.byName[i];
	
	comparison = rstrcasecmp(tagstr, t->shortname);

	if (comparison < 0)
	    u = i;
	else if (comparison > 0)
	    l = i + 1;
	else
	    return t->val;
    }
    return RPMTAG_NOT_FOUND;
}

const char * rpmTagGetName(rpmTagVal tag)
{
    return ((*rpmTags->tagName)(tag));
}

rpmTagType rpmTagGetType(rpmTagVal tag)
{
    return ((*rpmTags->tagType)(tag));
}

rpmTagType rpmTagGetTagType(rpmTagVal tag)
{
    return (rpmTagType)((*rpmTags->tagType)(tag) & RPM_MASK_TYPE);
}

rpmTagReturnType rpmTagGetReturnType(rpmTagVal tag)
{
    return ((*rpmTags->tagType)(tag) & RPM_MASK_RETURN_TYPE);
}

rpmTagClass rpmTagTypeGetClass(rpmTagType type)
{
    rpmTagClass tclass;
    switch (type & RPM_MASK_TYPE) {
    case RPM_CHAR_TYPE:
    case RPM_INT8_TYPE:
    case RPM_INT16_TYPE:
    case RPM_INT32_TYPE:
    case RPM_INT64_TYPE:
	tclass = RPM_NUMERIC_CLASS;
	break;
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	tclass = RPM_STRING_CLASS;
	break;
    case RPM_BIN_TYPE:
	tclass = RPM_BINARY_CLASS;
	break;
    case RPM_NULL_TYPE:
    default:
	tclass = RPM_NULL_CLASS;
	break;
    }
    return tclass;
}

rpmTagClass rpmTagGetClass(rpmTagVal tag)
{
    return rpmTagTypeGetClass(rpmTagGetTagType(tag));
}

rpmTagVal rpmTagGetValue(const char * tagstr)
{
    return ((*rpmTags->tagValue)(tagstr));
}

int rpmTagGetNames(rpmtd tagnames, int fullname)
{
    const char **names;
    const char *name;

    if (_rpmTags.byName == NULL)
	tagLoadIndex(&_rpmTags.byName, &_rpmTags.byNameSize, tagCmpName);
    if (tagnames == NULL ||_rpmTags.byName == NULL)
	return 0;

    rpmtdReset(tagnames);
    tagnames->count = _rpmTags.byNameSize;
    tagnames->data = names = xmalloc(tagnames->count * sizeof(*names));
    tagnames->type = RPM_STRING_ARRAY_TYPE;
    tagnames->flags = RPMTD_ALLOCED | RPMTD_IMMUTABLE;

    for (int i = 0; i < tagnames->count; i++) {
	name = fullname ? _rpmTags.byName[i]->name : 
			  _rpmTags.byName[i]->shortname;
	names[i] = name;
    }
    return tagnames->count;
}
