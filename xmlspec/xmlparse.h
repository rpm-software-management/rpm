#ifndef _XML_PARSE_H_
#define _XML_PARSE_H_

#include <expat.h>

#include "xmlstruct.h"

#define MAX_XML_LEVELS                  10
#define MAX_XML_ATTRS                   20

typedef struct structXMLParse
{
	char*            m_szXMLFile;
	XML_Parser       m_pParser;
	char*            m_szValue;
	int              m_nLevel;
	int              m_nLastGoodLevel;
	int              m_nErrors;
	int              m_nWarnings;
	int              m_naTree[MAX_XML_LEVELS];
	int              m_nVerbose;
	t_structXMLAttr* m_pAttrs;
	t_structXMLSpec* m_pSpec;
} t_structXMLParse;

#endif
