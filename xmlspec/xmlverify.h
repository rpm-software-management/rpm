#ifndef _XML_VERIFY_H_
#define _XML_VERIFY_H_

#include "xmlparse.h"

typedef enum enumXMLTagVals
{
	TAGVAL_SPEC                   = 0,
	TAGVAL_MACRO,
	TAGVAL_SOURCE,
	TAGVAL_SOURCEMIRROR,
	TAGVAL_PATCH,
	TAGVAL_PATCHMIRROR,
	TAGVAL_BUILDREQUIRES,
	TAGVAL_BUILDREQUIRE,
	TAGVAL_BUILDSUGGESTS,
	TAGVAL_BUILDSUGGEST,
	TAGVAL_PACKAGE,
	TAGVAL_PROVIDE,
	TAGVAL_PROVIDES,
	TAGVAL_REQUIRES,
	TAGVAL_REQUIRE,
	TAGVAL_OBSOLETES,
	TAGVAL_OBSOLETE,
	TAGVAL_SUGGESTS,
	TAGVAL_SUGGEST,
	TAGVAL_SUMMARY,
	TAGVAL_DESCRIPTION,
	TAGVAL_PRE,
	TAGVAL_POST,
	TAGVAL_PREUN,
	TAGVAL_POSTUN,
	TAGVAL_VERIFY,
	TAGVAL_FILES,
	TAGVAL_PREP,
	TAGVAL_BUILD,
	TAGVAL_INSTALL,
	TAGVAL_CLEAN,
	TAGVAL_SCRIPT,
	TAGVAL_SETUPMACRO,
	TAGVAL_PATCHMACRO,
	TAGVAL_CHANGELOG,
	TAGVAL_CHANGES,
	TAGVAL_CHANGE,
	TAGVAL_LAST                   = 0xFFFF
} t_enumXMLTagVals;

typedef enum enumXMLAttrTypes
{
	ATTRTYPE_ANY                  = 0,
	ATTRTYPE_ALPHA,
	ATTRTYPE_NUMERIC,
	ATTRTYPE_BOOL,
	ATTRTYPE_EMAIL,
	ATTRTYPE_DATE,
	ATTRTYPE_CMP,
	ATTRTYPE_SRCFILE,
	ATTRTYPE_SCRIPT,
	ATTRTYPE_URL,
	ATTRTYPE_MD5,
	ATTRTYPE_COUNTRY,
	ATTRTYPE_LAST                 = 0xFFFF
} t_enumXMLAttrTypes;

typedef struct structXMLAttrAttr
{
	char* m_szName;
	int   m_nType;
	int   m_nMandatory;
} t_structXMLAttrAttr;

typedef struct structXMLTags
{
	char*               m_szName;
	int                 m_nValue;
	int                 m_nLevel;
	int                 m_naTree[MAX_XML_LEVELS];
	t_structXMLAttrAttr m_saAttrs[MAX_XML_ATTRS];
} t_structXMLTags;

#ifndef NO_XMLTAGS_EXTERN
extern t_structXMLTags g_pXMLTags[];
#endif

int verifyXMLPath(const char* szName,
		  t_structXMLParse* pParse);

int verifyXMLAttr(const char* szAttr,
		  t_structXMLParse* pParse,
		  int nTagPos);

int verifyXMLAttrs(const char** szaAttrs,
		   t_structXMLParse* pParse,
		   int nTagPos);

#endif

