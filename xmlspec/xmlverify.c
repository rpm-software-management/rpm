#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// This is a hack to make sure we can include the
// xmlverify header file
#define NO_XMLTAGS_EXTERN               1

#include "xmlparse.h"
#include "xmlverify.h"

extern char* g_szSpecPath;

t_structXMLTags g_pXMLTags[] =
{
	{
		"spec", TAGVAL_SPEC, 0,
		{
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{"version",        ATTRTYPE_ANY,     1},
			{"release",        ATTRTYPE_ANY,     1},
			{"epoch",          ATTRTYPE_NUMERIC, 0},
			{"buildroot",      ATTRTYPE_ANY,     0},
			{"builddir",       ATTRTYPE_ANY,     0},
			{"url",            ATTRTYPE_URL,     0},
			{"copyright",      ATTRTYPE_ANY,     0},
			{"distribution",   ATTRTYPE_ANY,     1},
			{"vendor",         ATTRTYPE_ANY,     1},
			{"packager",       ATTRTYPE_ANY,     1},
			{"packager-email", ATTRTYPE_EMAIL,   0},
			{NULL,             ATTRTYPE_ANY,     1}
		},
	},
	{
		"macro", TAGVAL_MACRO, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"source", TAGVAL_SOURCE, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_SRCFILE, 1},
			{"size",           ATTRTYPE_NUMERIC, 0},
			{"md5",            ATTRTYPE_MD5,     0},
			{"path",           ATTRTYPE_ANY,     0},
			{"number",         ATTRTYPE_NUMERIC, 0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"mirror", TAGVAL_SOURCEMIRROR, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_SOURCE,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     0},
			{"location",       ATTRTYPE_ANY,     0},
			{"country",        ATTRTYPE_COUNTRY, 0},
			{"path",           ATTRTYPE_URL,     1},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},

	{
		"patch", TAGVAL_PATCH, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_SRCFILE, 1},
			{"size",           ATTRTYPE_NUMERIC, 0},
			{"md5",            ATTRTYPE_MD5,     0},
			{"path",           ATTRTYPE_ANY,     0},
			{"number",         ATTRTYPE_NUMERIC, 0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"mirror", TAGVAL_PATCHMIRROR, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PATCH,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     0},
			{"location",       ATTRTYPE_ANY,     0},
			{"country",        ATTRTYPE_COUNTRY, 0},
			{"path",           ATTRTYPE_URL,     1},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"buildrequires", TAGVAL_BUILDREQUIRES, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"require", TAGVAL_BUILDREQUIRE, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_BUILDREQUIRES,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{"version",        ATTRTYPE_ANY,     0},
			{"cmp",            ATTRTYPE_CMP,     0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"buildsuggests", TAGVAL_BUILDSUGGESTS, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"suggest", TAGVAL_BUILDSUGGEST, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_BUILDSUGGESTS,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{"version",        ATTRTYPE_ANY,     0},
			{"cmp",            ATTRTYPE_CMP,     0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{	"package", TAGVAL_PACKAGE, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     0},
			{"group",          ATTRTYPE_ANY,     1},
			{"autoreqprov",    ATTRTYPE_BOOL,    0},
			{"autoprov",       ATTRTYPE_BOOL,    0},
			{"autoreq",        ATTRTYPE_BOOL,    0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"requires", TAGVAL_REQUIRES, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"require", TAGVAL_REQUIRE, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_REQUIRES,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{"version",        ATTRTYPE_ANY,     0},
			{"cmp",            ATTRTYPE_CMP,     0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"provides", TAGVAL_PROVIDES, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"provide", TAGVAL_PROVIDE, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_PROVIDES,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"obsoletes", TAGVAL_OBSOLETES, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"obsolete", TAGVAL_OBSOLETE, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_OBSOLETES,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"suggests", TAGVAL_SUGGESTS, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"suggest", TAGVAL_SUGGEST, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_SUGGESTS,
			TAGVAL_LAST
		},
		{
			{"name",           ATTRTYPE_ANY,     1},
			{"version",        ATTRTYPE_ANY,     0},
			{"cmp",            ATTRTYPE_CMP,     0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"summary", TAGVAL_SUMMARY, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"description", TAGVAL_DESCRIPTION, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"pre", TAGVAL_PRE, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_PRE,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"post", TAGVAL_POST, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_POST,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"preun", TAGVAL_PREUN, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_PREUN,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"postun", TAGVAL_POSTUN, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_POSTUN,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{"verify", TAGVAL_VERIFY, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_VERIFY,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"files", TAGVAL_FILES, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PACKAGE,
			TAGVAL_LAST
		},
		{
			{"uid",            ATTRTYPE_ANY,     0},
			{"gid",            ATTRTYPE_ANY,     0},
			{"list",           ATTRTYPE_SCRIPT,  1},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"prep", TAGVAL_PREP, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PREP,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"setup", TAGVAL_SETUPMACRO, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PREP,
			TAGVAL_LAST
		},
		{
			{"source",         ATTRTYPE_NUMERIC, 0},
			{"path",           ATTRTYPE_ANY,     0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"patch", TAGVAL_PATCHMACRO, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_PREP,
			TAGVAL_LAST
		},
		{
			{"patch",          ATTRTYPE_NUMERIC, 0},
			{"level",          ATTRTYPE_NUMERIC, 0},
			{"path",           ATTRTYPE_ANY,     0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"build", TAGVAL_BUILD, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_BUILD,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"install", TAGVAL_INSTALL, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_INSTALL,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"clean", TAGVAL_CLEAN, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"script", TAGVAL_SCRIPT, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_CLEAN,
			TAGVAL_LAST
		},
		{
			{"interpreter",    ATTRTYPE_ANY,     0},
			{"script",         ATTRTYPE_SCRIPT,  0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"changelog", TAGVAL_CHANGELOG, 1,
		{
			TAGVAL_SPEC,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"changes", TAGVAL_CHANGES, 2,
		{
			TAGVAL_SPEC,
			TAGVAL_CHANGELOG,
			TAGVAL_LAST
		},
		{
			{"date",           ATTRTYPE_ANY,     1},
			{"version",        ATTRTYPE_ANY,     0},
			{"author",         ATTRTYPE_ANY,     0},
			{"author-email",   ATTRTYPE_EMAIL,   0},
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		"change", TAGVAL_CHANGE, 3,
		{
			TAGVAL_SPEC,
			TAGVAL_CHANGELOG,
			TAGVAL_CHANGES,
			TAGVAL_CHANGE,
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	},
	{
		NULL, TAGVAL_LAST, 0,
		{
			TAGVAL_LAST
		},
		{
			{NULL,             ATTRTYPE_ANY,     1}
		}
	}
};

int verifyXMLPath(const char* szName,
		  t_structXMLParse* pParse)
{
	int nFound = -1;
	int i = 0;
	int j = 0;
	int nValue = 0;

	if (pParse->m_nLevel == MAX_XML_LEVELS)
		return -1;

	while ((nFound == -1) && (g_pXMLTags[i].m_nValue != TAGVAL_LAST)) {
		if (g_pXMLTags[i].m_nLevel == pParse->m_nLevel) {
			if (!strcasecmp(szName, g_pXMLTags[i].m_szName)) {
				nFound = i;
				for (j = 0; j < pParse->m_nLevel; j++) {
					// NB: Remember that we are storing the position
					// of the tag, not the actualv value!!!
					nValue = g_pXMLTags[pParse->m_naTree[j]].m_nValue;
					if (nValue != g_pXMLTags[i].m_naTree[j])
						nFound = -1;
				}
			}
		}
		i++;
	}

	if (nFound == -1) {
		pParse->m_nWarnings++;
		fprintf(stderr, "warning: %s(%d): Unexpected XML structure/tag \"%s\" found. (Ignoring)\n",
			pParse->m_szXMLFile,
			XML_GetCurrentLineNumber(pParse->m_pParser), szName);
	}

	return nFound;
}

int verifyXMLAttr(const char* szAttr,
		  t_structXMLParse* pParse,
		  int nTagPos)
{
	int nFound = -1;
	int i = 0;

	while ((nFound == -1) && (g_pXMLTags[nTagPos].m_saAttrs[i].m_szName)) {
		if (!strcasecmp(szAttr, g_pXMLTags[nTagPos].m_saAttrs[i].m_szName))
			nFound = i;
		i++;
	}

	if (nFound == -1) {
		pParse->m_nWarnings++;
		fprintf(stderr, "warning: %s(%d): Unexpected tag attribute \"%s\" for tag \"%s\" found.\n",
			pParse->m_szXMLFile,
			XML_GetCurrentLineNumber(pParse->m_pParser),
			szAttr, g_pXMLTags[nTagPos].m_szName);
	}

	return nFound;
}

int verifyXMLAttrType(const t_structXMLAttr* pAttr,
		      const t_structXMLAttrAttr* pAttrAttr,
		      t_structXMLParse* pParse,
		      int nTagPos)
{
	int i = 0;
	int nError = 0;
	char* szTmp;
	char* szOld;
	FILE* fTmp;

	if (!pAttr->m_szValue || !strlen(pAttr->m_szValue)) {
		pParse->m_nErrors++;
		fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" does not have a value.\n",
			pParse->m_szXMLFile,
			XML_GetCurrentLineNumber(pParse->m_pParser),
			pAttr->m_szName,
			g_pXMLTags[nTagPos].m_szName);
		return ++nError;
	}

	switch (pAttrAttr->m_nType) {
		case ATTRTYPE_ALPHA:
			while ((!nError) && (pAttr->m_szValue[i])) {
				if (!isalpha(pAttr->m_szValue[i])) {
					fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" contains non-alphabetic characters.\n",
						pParse->m_szXMLFile,
						XML_GetCurrentLineNumber(pParse->m_pParser),
						pAttr->m_szName,
						g_pXMLTags[nTagPos].m_szName);
					nError++;
				}
				i++;
			}
			break;
		case ATTRTYPE_NUMERIC:
			while ((!nError) && (pAttr->m_szValue[i])) {
				if (!isdigit(pAttr->m_szValue[i])) {
					fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" contains non-numeric characters.\n",
						pParse->m_szXMLFile,
						XML_GetCurrentLineNumber(pParse->m_pParser),
						pAttr->m_szName,
						g_pXMLTags[nTagPos].m_szName);
					nError++;
				}
				i++;
			}
			break;
		case ATTRTYPE_BOOL:
			nError++;
			if (!strcasecmp(pAttr->m_szValue, "yes") ||
			    !strcasecmp(pAttr->m_szValue, "no") ||
			    !strcasecmp(pAttr->m_szValue, "true") ||
			    !strcasecmp(pAttr->m_szValue, "false") ||
			    !strcasecmp(pAttr->m_szValue, "0") ||
			    !strcasecmp(pAttr->m_szValue, "1"))
				nError = 0;
			if (nError) {
				fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" does not contain a valid boolean value. Allowed values are: \"yes\"/\"no\", \"true\"/\"false\" or \"1\"/\"0\" combinations.\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser),
					pAttr->m_szName,
					g_pXMLTags[nTagPos].m_szName);
			}
			break;
		case ATTRTYPE_EMAIL:
			break;
		case ATTRTYPE_DATE:
			break;
		case ATTRTYPE_CMP:
			nError++;
			if (!strcasecmp(pAttr->m_szValue, "eq") ||
			    !strcasecmp(pAttr->m_szValue, "lt") ||
			    !strcasecmp(pAttr->m_szValue, "le") ||
			    !strcasecmp(pAttr->m_szValue, "gt") ||
			    !strcasecmp(pAttr->m_szValue, "ge"))
				nError = 0;
			if (nError) {
				fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" does not contain a valid comparison value. Allowed values are: \"eq\" (equal), \"lt\" (less), \"le\" (less or equal), \"gt\" (greater) and \"ge\" (greater or equal).\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser),
					pAttr->m_szName,
					g_pXMLTags[nTagPos].m_szName);
			}
			break;
		case ATTRTYPE_SCRIPT:
			/*nError++;
			szTmp = NULL;
			szOld = NULL;
			newStr(pAttr->m_szValue, &szOld);
			if (g_szSpecPath) {
				szTmp = malloc(strlen(g_szSpecPath)+strlen(szOld)+2);
				sprintf(szTmp, "%s/%s", g_szSpecPath, szOld);
			}
			else {
				szTmp = malloc(strlen(szOld)+2);
				sprintf(szTmp, "%s", szOld);
			}
			newStrEx(szTmp, (char**)&(pAttr->m_szValue));
			if ((fTmp = fopen(pAttr->m_szValue, "r"))) {
				nError = 0;
				fclose(fTmp);
				freeStr(&szOld);
			}
			freeStr(&szTmp);
			if (!nError)
				break;
			newStrEx(szOld, (char**)&(pAttr->m_szValue));
			freeStr(&szOld);
			nError = 0;*/
		case ATTRTYPE_SRCFILE:
			nError++;
			szTmp = NULL;
			szOld = NULL;
			newStr(pAttr->m_szValue, &szOld);
			szTmp = malloc(strlen("%%{_sourcedir}")+strlen(pAttr->m_szValue)+2);
			sprintf(szTmp, "%%{_sourcedir}/%s", szOld);
			newStrEx(szTmp, (char**)&(pAttr->m_szValue));
			if ((fTmp = fopen(pAttr->m_szValue, "r"))) {
				nError = 0;
				fclose(fTmp);
			}

			if (nError) {
				fprintf(stderr, "error: %s(%d): File \"%s\" (attribute \"%s\" for tag \"%s\") does not exist.\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser),
					pAttr->m_szValue,
					pAttr->m_szName,
					g_pXMLTags[nTagPos].m_szName);
				newStr(szOld, (char**)&(pAttr->m_szValue));
			}
			freeStr(&szTmp);
			freeStr(&szOld);
			break;
		case ATTRTYPE_URL:
			break;
		case ATTRTYPE_MD5:
			if (strlen(pAttr->m_szValue) != 32) {
				fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" does not contain a valid MD5 sum.\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser),
					pAttr->m_szName,
					g_pXMLTags[nTagPos].m_szName);
				nError++;
			}
			break;
		case ATTRTYPE_COUNTRY:
			if (strlen(pAttr->m_szValue) != 2) {
				fprintf(stderr, "error: %s(%d): Attribute \"%s\" for tag \"%s\" does not contain a valid country code.\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser),
					pAttr->m_szName,
					g_pXMLTags[nTagPos].m_szName);
				nError++;
			}
		case ATTRTYPE_ANY:
		default:
			break;
	}

	return nError;
}

int verifyXMLAttrs(const char** szaAttrs,
		   t_structXMLParse* pParse,
		   int nTagPos)

{
	int i = 0;
	int nRet = 1;
	int nFound = 0;
	int nAttrPos = 0;
	t_structXMLAttr* pAttr = NULL;

	// add all tags
	freeXMLAttr(&(pParse->m_pAttrs));
	for (i = 0; szaAttrs[i]; i += 2) {
		if ((nAttrPos = verifyXMLAttr(szaAttrs[i], pParse, nTagPos)) != -1)
			addXMLAttr(szaAttrs[i], szaAttrs[i+1],
				   g_pXMLTags[nTagPos].m_saAttrs[nAttrPos].m_nType,
				   &(pParse->m_pAttrs));
	}

	// verify mandatory tags and check attribute type
	for (i = 0; g_pXMLTags[nTagPos].m_saAttrs[i].m_szName; i++) {
		pAttr = pParse->m_pAttrs;
		nFound = 0;
		while ((!nFound) && (pAttr)) {
			if (!strcasecmp(pAttr->m_szName,
					g_pXMLTags[nTagPos].m_saAttrs[i].m_szName)) {
				nFound = 1;
				pParse->m_nErrors += verifyXMLAttrType(pAttr,
						     &(g_pXMLTags[nTagPos].m_saAttrs[i]),
						     pParse, nTagPos);
			}
			else
				pAttr = pAttr->m_pNext;
		}
		if ((!nFound) && (g_pXMLTags[nTagPos].m_saAttrs[i].m_nMandatory)) {
			pParse->m_nErrors++;
			fprintf(stderr, "error: %s(%d): Mandatory attribute (%s) for tag \"%s\" not found.\n",
				pParse->m_szXMLFile,
				XML_GetCurrentLineNumber(pParse->m_pParser),
				g_pXMLTags[nTagPos].m_saAttrs[i].m_szName,
				g_pXMLTags[nTagPos].m_szName);
			nRet = -1;
		}
	}

	return nRet;
}
