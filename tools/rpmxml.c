#include "system.h"
#include <rpmlib.h>
#define	_RPMXP_INTERNAL
#include "rpmxp.h"
#include "debug.h"

int _rpmxp_debug = 0;

const char * rpmxpDTD = "\
<?xml version=\"1.0\"?>\n\
<!DOCTYPE rpmHeader [\n\
<!ELEMENT rpmHeader (rpmTag+)>\n\
<!ELEMENT rpmTag	(string+|integer+|base64+)>\n\
<!ATTLIST rpmTag name	CDATA #REQUIRED>\n\
<!ELEMENT string	(#PCDATA)>\n\
<!ELEMENT integer	(#PCDATA)>\n\
<!ELEMENT base64	(#PCDATA)>\n\
]>\n\
";

rpmxp rpmxpFree(rpmxp xp)
{
    if (xp != NULL) {
	if (xp->value) {
	    xmlFree(xp->value);
	    xp->value = NULL;
	}
	if (xp->name) {
	    xmlFree(xp->name);
	    xp->name = NULL;
	}
	if (xp->reader != NULL) {
	    xmlFreeTextReader(xp->reader);
	    xp->reader = NULL;
	}
	free(xp);
	xp = NULL;
    }
    return xp;
}

rpmxp rpmxpNew(const char * fn)
{
    rpmxp xp = calloc(1, sizeof(*xp));

    if (xp == NULL)
	return NULL;
    if (fn) {
	int xx;
	xp->reader = xmlNewTextReaderFilename(fn);
	if (xp->reader == NULL)
	    return rpmxpFree(xp);
	xx = xmlTextReaderSetParserProp(xp->reader, XML_PARSER_VALIDATE, 1);
	xx = xmlTextReaderSetParserProp(xp->reader, XML_PARSER_SUBST_ENTITIES, 1);
    }
    xp->name = NULL;
    xp->value = NULL;
    xp->depth = 0;
    xp->nodeType = 0;
    xp->isEmptyElement = 0;
    xp->n = 2;
    return xp;
}

/**
 * Return tag table entry from name lookup.
 * @todo bsearch on sorted name table.
 * @param tbl		tag table
 * @param name		tag name to find
 * @return		tag value, 0 on not found
 */
static headerTagTableEntry myTagByName(headerTagTableEntry tbl, const char * name)
{
    int x = (strncmp(name, "RPMTAG_", (sizeof("RPMTAG_")-1))
		? (sizeof("RPMTAG_")-1) : 0);

    for (; tbl->name != NULL; tbl++) {
	if (!xstrcasecmp(tbl->name+x, name))
	    return tbl;
    }
    return NULL;
}

int rpmxpRead(rpmxp xp)
{
    return xmlTextReaderRead(xp->reader);
}

int rpmxpProcess(rpmxp xp)
{
    int rc = 0;

    xp->name = xmlTextReaderName(xp->reader);
    xp->value = xmlTextReaderValue(xp->reader);
    xp->depth = xmlTextReaderDepth(xp->reader);
    xp->nodeType = xmlTextReaderNodeType(xp->reader);
    xp->isEmptyElement = xmlTextReaderIsEmptyElement(xp->reader);

    if (xp->name == NULL)
	xp->name = xmlStrdup(BAD_CAST "--");

if (_rpmxp_debug)
printf("%d %d %s %d\n", xp->depth, xp->nodeType, xp->name, xp->isEmptyElement);
    switch (xp->nodeType) {
    case XML_READER_TYPE_ELEMENT:
	printf("%*s<%s", (xp->n * xp->depth), "", xp->name);
	while (xmlTextReaderMoveToNextAttribute(xp->reader) != 0) {
	    xmlChar * attrN = xmlTextReaderName(xp->reader);
	    xmlChar * attrV = xmlTextReaderValue(xp->reader);
	    printf(" %s", attrN);
	    if (attrV) {
		printf("=\"%s\"", attrV);
		if (!strcmp(xp->name, "rpmTag") && !strcmp(attrN, "name")) {
		    /* XXX rpmSTagTable gonna be needed. */
		    xp->tte = myTagByName(rpmTagTable, attrV);
		}
		xmlFree(attrV);
	    }
	    xmlFree(attrN);
	}
	if (xp->isEmptyElement) {
	    printf("/>\n");
	    if (xp->h && xp->tte) {
		headerTagTableEntry tte = xp->tte;
		if (!strcmp(xp->name, "string")) {
		    const char * nstr = "";
		    (void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &nstr, 1);
		} else
		if (!strcmp(xp->name, "integer")) {
		    int i = 0;
		    (void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &i, 1);
		}
	    }
	} else {
	    printf(">");
	    if (xp->depth < 2)
		printf("\n");
	}
	if (!strcmp(xp->name, "rpmHeader")) {
	    xp->h = headerNew();
	}
	break;
    case XML_READER_TYPE_END_ELEMENT:
	if (xp->depth < 2)
	    printf("%*s", (xp->n * xp->depth), "");
	printf("</%s>\n", xp->name);
	if (!strcmp(xp->name, "rpmHeader")) {
	    if (xp->h) {
		FD_t fdo = Fopen("time.xmlhdr", "w.ufdio");
		if (fdo != NULL) {
		    headerWrite(fdo, xp->h, HEADER_MAGIC_YES);
		    Fclose(fdo);
		}
	    }
	    xp->h = headerFree(xp->h);
	} else
	if (!strcmp(xp->name, "rpmTag")) {
	    xp->tte = NULL;
	}
	break;
    case XML_READER_TYPE_TEXT:
	printf("%s", xp->value);
	if (xp->h && xp->tte) {
	    headerTagTableEntry tte = xp->tte;
	    switch (tte->type) {
	    case RPM_NULL_TYPE:
	    {
	    }	break;
	    case RPM_CHAR_TYPE:
	    {	char c = xp->value[0];
		(void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &c, 1);
	    }	break;
	    case RPM_INT8_TYPE:
	    {	int_8 i = strtol(xp->value, NULL, 0);
		(void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &i, 1);
	    }	break;
	    case RPM_INT16_TYPE:
	    {	int_16 i = strtol(xp->value, NULL, 0);
		(void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &i, 1);
	    }	break;
	    case RPM_INT32_TYPE:
	    {	int_32 i = strtol(xp->value, NULL, 0);
		(void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &i, 1);
	    }	break;
	    case RPM_STRING_TYPE:
	    {	const char * s = xp->value;
		(void) headerAddEntry(xp->h, tte->val, tte->type, s, 1);
	    }	break;
	    case RPM_STRING_ARRAY_TYPE:
	    {	const char * s = xp->value;
		(void) headerAddOrAppendEntry(xp->h, tte->val, tte->type, &s, 1);
	    }	break;
	    case RPM_I18NSTRING_TYPE:
	    {	const char * s = xp->value;
		(void) headerAddI18NString(xp->h, tte->val, s, "C");
	    }	break;
	    default:
		break;
	    }
	}
	break;
    case XML_READER_TYPE_DOCUMENT_TYPE:
	break;
    case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
	break;

    case XML_READER_TYPE_NONE:
    case XML_READER_TYPE_ATTRIBUTE:
    case XML_READER_TYPE_CDATA:
    case XML_READER_TYPE_ENTITY_REFERENCE:
    case XML_READER_TYPE_ENTITY:
    case XML_READER_TYPE_PROCESSING_INSTRUCTION:
    case XML_READER_TYPE_COMMENT:
    case XML_READER_TYPE_DOCUMENT:
    case XML_READER_TYPE_DOCUMENT_FRAGMENT:
    case XML_READER_TYPE_NOTATION:
    case XML_READER_TYPE_WHITESPACE:
    case XML_READER_TYPE_END_ENTITY:
    case XML_READER_TYPE_XML_DECLARATION:
    default:
	printf("%d %d %s %d\n", xp->depth, xp->nodeType,
		xp->name, xp->isEmptyElement);
	if (xp->value)
	    printf(" %s", xp->value);
	if (xp->depth < 2)
	    printf("\n");
	rc = -1;
	break;
    }


    if (xp->value != NULL) {
        xmlFree(xp->value);
	xp->value = NULL;
    }
    if (xp->name != NULL) {
	xmlFree(xp->name);
	xp->name = NULL;
    }
    return rc;
}

int rpmxpParseFile(rpmxp xp)
{
    int ret = -1;

    if (xp != NULL)
    while ((ret = rpmxpRead(xp)) == 1)
	rpmxpProcess(xp);
    return ret;
}

int main(int argc, char ** argv)
{
    const char * fn = "time.xml";
    rpmxp xp;
    int ec = 0;

    xp = rpmxpNew(fn);
    rpmxpParseFile(xp);
    xp = rpmxpFree(xp);

    return ec;
}
