#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpmio.h"
#include "header.h"
#include "stringbuf.h"
#include "rpmspec.h"
#include "rpmbuild.h"
#include "header_internal.h"

#include "xmlmisc.h"

extern Spec g_pSpec;

void freeStr(char** pszStr)
{
	if (*pszStr != NULL)
		free(*pszStr);
	*pszStr = NULL;
}

char* newStr(const char* szValue,
	     char** pszStr)
{
	freeStr(pszStr);
	if (szValue) {
		*pszStr = malloc((strlen(szValue)+1)*sizeof(char));
		sprintf(*pszStr, "%s", szValue);
	}

	return *pszStr;
}

char* newStrEx(const char* szValue,
	       char** pszStr)
{
	char* szBuffer = NULL;
	int nLen;

	if (g_pSpec == NULL)
		return newStr(szValue, pszStr);

	if (szValue) {
		nLen = (strlen(szValue)*2)+1024;
		szBuffer = malloc(nLen+1);
		sprintf(szBuffer, "%s", szValue);
		expandMacros(g_pSpec, g_pSpec->macros, szBuffer, nLen);
		freeStr(pszStr);
		newStr(szBuffer, pszStr);
		free(szBuffer);
	}
	else
		freeStr(pszStr);

	return *pszStr;
}

void addMacroEx(char* szName, char** pszVar, int nLevel)
{
	if (g_pSpec && g_pSpec->macros)
		addMacro(g_pSpec->macros, szName, NULL, newStrEx(*pszVar, pszVar), nLevel);
}

int strToBool(const char* szBool)
{
	int nBool = 0;

	if (!strcasecmp(szBool, "on"))
		nBool = 1;
	else if (!strcasecmp(szBool, "off"))
		nBool = 0;
	else if (!strcasecmp(szBool, "true"))
		nBool = 1;
	else if (!strcasecmp(szBool, "false"))
		nBool = 0;
	else if (!strcasecmp(szBool, "yes"))
		nBool = 1;
	else if (!strcasecmp(szBool, "no"))
		nBool = 0;
	else if (!strcasecmp(szBool, "1"))
		nBool = 1;
	else if (!strcasecmp(szBool, "0"))
		nBool = 0;

	return nBool;
}

StringBuf fileToStrBuf(const char* szFile,
		       const char* szPrepend)
{
	FILE* fIn;
	StringBuf pSb = NULL;
	StringBuf pTmp;
	int nLen;
	char szBuffer[1025];
	char* szTmp = NULL;
	char** szaLines, **szaStart;

	if ((szFile) &&
	    (fIn = fopen(szFile, "r"))) {
		pTmp = newStringBuf();
		if (szPrepend)
			appendLineStringBuf(pTmp, szPrepend);
		while (!feof(fIn)) {
			nLen = fread(szBuffer, sizeof(char), 1024, fIn);
			szBuffer[nLen] = '\0';
			appendStringBuf(pTmp, szBuffer);
		}
		appendLineStringBuf(pTmp, "");
		fclose(fIn);
		szaStart = splitString(getStringBuf(pTmp), strlen(getStringBuf(pTmp)), '\n');
		//newStrEx(getStringBuf(pSb), &szTmp);
		//freeStringBuf(pSb);

		pSb = newStringBuf();
		for (szaLines = szaStart; *szaLines; szaLines++) {
			if (!strncmp(*szaLines, "%setup", sizeof("%setup")-1)) {
				//doSetupMacro(spec, *sazLines);
			}
			else if (!strncmp(*szaLines, "%patch", sizeof("%patch")-1)) {
	    			//doPatchMacro(spec, *szaLines);
			}
			else {
				newStrEx(*szaLines, &szTmp);
	    			appendLineStringBuf(pSb, szTmp);
				freeStr(&szTmp);
			}
		}
		//appendStringBuf(pSb, szTmp);
		//freeStr(&szTmp);
		freeSplitString(szaStart);
		freeStringBuf(pTmp);
	}

	return pSb;
}

char* fileToStr(const char* szFile,
		const char* szPrepend)
{
	StringBuf pSb;
	char* szRet = NULL;

	if ((pSb = fileToStrBuf(szFile, szPrepend))) {
		newStr(getStringBuf(pSb), &szRet);
		freeStringBuf(pSb);
	}

	return szRet;
}
