#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <expat.h>

#include "rpmbuild.h"

#include "xmlparse.h"
#include "xmlmisc.h"
#include "xmlverify.h"
#include "xmlstruct.h"

void handleXMLStartLevel0(t_structXMLParse* pParse)
{
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_SPEC:
			pParse->m_pSpec = newXMLSpec(pParse->m_pAttrs, pParse->m_szXMLFile);
			break;
		default:
			fprintf(stderr, "warning: %s(%d): Ignoring tag \"%s\" on level %d. (Function not implemented)\n",
				pParse->m_szXMLFile,
				XML_GetCurrentLineNumber(pParse->m_pParser),
				g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_szName,
				pParse->m_nLevel);
			pParse->m_nWarnings++;
			break;
	}
}

/*f (!strcasecmp(szName, "pre")) {
			pPkg = getLastXMLPackage(pParse->m_pSpec->m_pPackages);
			pPkg->m_pPre = newXMLScript(attrToStruct(szaAttrs, &(pParse->m_pAttrs)));
		}*/
void handleXMLStartLevel1(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_MACRO:
			addXMLMacro(pParse->m_pAttrs, &(pSpec->m_pMacros));
			break;
		case TAGVAL_SOURCE:
			addXMLSource(pParse->m_pAttrs, &(pSpec->m_pSources));
			break;
		case TAGVAL_PATCH:
			addXMLSource(pParse->m_pAttrs, &(pSpec->m_pPatches));
			break;
		case TAGVAL_PACKAGE:
			addXMLPackage(pParse->m_pAttrs, &(pSpec->m_pPackages));
			break;
		case TAGVAL_PREP:
			pSpec->m_pPrep = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_BUILD:
			pSpec->m_pBuild = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_INSTALL:
			pSpec->m_pInstall = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_CLEAN:
			pSpec->m_pClean = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_BUILDREQUIRES:
		case TAGVAL_CHANGELOG:
			// we don't need to do anything
			break;
		default:
			fprintf(stderr, "warning: %s(%d): Ignoring tag \"%s\" on level %d. (Function not implemented)\n",
				pParse->m_szXMLFile,
				XML_GetCurrentLineNumber(pParse->m_pParser),
				g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_szName,
				pParse->m_nLevel);
			pParse->m_nWarnings++;
			break;
	}
}

void handleXMLStartLevel2(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;
	t_structXMLSource* pSource = NULL;
	t_structXMLPackage* pPkg = NULL;
	t_structXMLScript* pScript = NULL;
	t_structXMLAttr* pAttr = NULL;
	StringBuf pSb = NULL;
	int nNum = 0;
	int nLevel = 0;
	char* szTmp = NULL;
	char* szPath = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_SOURCEMIRROR:
			pSource = getLastXMLSource(pSpec->m_pSources);
			addXMLMirror(pParse->m_pAttrs, &(pSource->m_pMirrors));
			break;
		case TAGVAL_PATCHMIRROR:
			pSource = getLastXMLSource(pSpec->m_pPatches);
			addXMLMirror(pParse->m_pAttrs, &(pSource->m_pMirrors));
			break;
		case TAGVAL_BUILDREQUIRE:
			addXMLRequire(pParse->m_pAttrs, &(pSpec->m_pBuildRequires));
			break;
		case TAGVAL_CHANGES:
			addXMLChanges(pParse->m_pAttrs, &(pSpec->m_pChangelog));
			break;
		case TAGVAL_PRE:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			pPkg->m_pPre = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_POST:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			pPkg->m_pPost = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_PREUN:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			pPkg->m_pPreUn = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_POSTUN:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			pPkg->m_pPostUn = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_VERIFY:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			pPkg->m_pVerify = newXMLScripts(pParse->m_pAttrs);
			break;
		case TAGVAL_FILES:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			pPkg->m_pFiles = newXMLFiles(pParse->m_pAttrs);
			break;
		case TAGVAL_SCRIPT: // prep, build, install, clean
			switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel-1]].m_nValue) {
				case TAGVAL_PREP:
					addXMLScript(pParse->m_pAttrs,
						     &(pSpec->m_pPrep->m_pScripts));
					break;
				case TAGVAL_BUILD:
					addXMLScript(pParse->m_pAttrs,
						     &(pSpec->m_pBuild->m_pScripts));
					break;
				case TAGVAL_INSTALL:
					addXMLScript(pParse->m_pAttrs,
						     &(pSpec->m_pInstall->m_pScripts));
					break;
				case TAGVAL_CLEAN:
					addXMLScript(pParse->m_pAttrs,
						     &(pSpec->m_pClean->m_pScripts));
					break;
				default:
					break;
			}
			break;
		case TAGVAL_SETUPMACRO:
			pScript = addXMLScript(NULL, &(pSpec->m_pPrep->m_pScripts));
			if ((pAttr = getXMLAttr("source", pParse->m_pAttrs)))
				attrSetInt(pAttr, "source", &nNum);
			if ((pAttr = getXMLAttr("path", pParse->m_pAttrs)))
				attrSetStr(pAttr, "path", &szPath);
			else
				newStrEx(pParse->m_pSpec->m_szBuildSubdir, &szPath);
			if ((pSource = getXMLSource(nNum, pSpec->m_pSources))) {
				nNum = strlen(pSource->m_szName);
				szTmp = pSource->m_szName+(nNum-3);
				pSb = newStringBuf();
				nNum = 1;
				if (!strcasecmp(szTmp, "bz2")) {
					szTmp = NULL;
					newStrEx("%{_bzip2bin} -dc ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
				}
				else if (!strcasecmp(szTmp, ".gz")) {
					szTmp = NULL;
					newStrEx("%{_gzipbin} -dc ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
				}
				else if (!strcasecmp(szTmp, "zip")) {
					szTmp = NULL;
					newStrEx("%{_unzipbin} ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
					nNum = 0;
				}
				else if (!strcasecmp(szTmp, "tar")) {
					szTmp = NULL;
					newStrEx("cat ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
				}
				else {
					fprintf(stderr, "error: %s(%d): Invalid compressed format for source in \"setup\" script.\n",
						pParse->m_szXMLFile,
						XML_GetCurrentLineNumber(pParse->m_pParser));
					pParse->m_nErrors++;
					freeStr(&szPath);
					freeStringBuf(pSb);
					return;
				}
				if (nNum && pParse->m_nVerbose)
					appendStringBuf(pSb, " | tar -xvvf -");
				else if (nNum)
					appendStringBuf(pSb, " | tar -xf -");
				appendLineStringBuf(pSb, "");
				appendLineStringBuf(pSb, "STATUS=$?");
				appendLineStringBuf(pSb, "if [ $STATUS -ne 0 ]; then");
				appendLineStringBuf(pSb, "    exit $STATUS");
				appendLineStringBuf(pSb, "fi");
				appendStringBuf(pSb, "cd ");
				appendStringBuf(pSb, szPath);
				appendLineStringBuf(pSb, "");
				newStr(getStringBuf(pSb), (char**)&(pScript->m_szEntry));
				freeStringBuf(pSb);
				freeStr(&szTmp);
				freeStr(&szPath);
			}
			else {
				freeStr(&szPath);
				fprintf(stderr, "error: %s(%d): Invalid \"setup\" script. Source not available.\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser));
				pParse->m_nErrors++;
			}
			break;
		case TAGVAL_PATCHMACRO:
			pScript = addXMLScript(NULL, &(pSpec->m_pPrep->m_pScripts));
			if ((pAttr = getXMLAttr("patch", pParse->m_pAttrs)))
				attrSetInt(pAttr, "patch", &nNum);
			if ((pAttr = getXMLAttr("path", pParse->m_pAttrs)))
				attrSetStr(pAttr, "path", &szPath);
			else
				newStrEx(pParse->m_pSpec->m_szBuildSubdir, &szPath);
			if ((pAttr = getXMLAttr("level", pParse->m_pAttrs)))
				attrSetInt(pAttr, "level", &nLevel);
			if ((pSource = getXMLSource(nNum, pSpec->m_pPatches))) {
				pSb = newStringBuf();
				newStrEx("cd %{_builddir}", &szTmp);
				appendLineStringBuf(pSb, szTmp);
				freeStr(&szTmp);
				appendStringBuf(pSb, "cd ");
				appendStringBuf(pSb, szPath);
				appendLineStringBuf(pSb, "");
				nNum = strlen(pSource->m_szName);
				szTmp = pSource->m_szName+(nNum-3);
				nNum = 1;
				if (!strcasecmp(szTmp, "bz2")) {
					szTmp = NULL;
					newStrEx("%{_bzip2bin} -dc ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
				}
				else if (!strcasecmp(szTmp, ".gz")) {
					szTmp = NULL;
					newStrEx("%{_gzipbin} -dc ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
				}
				else {
					szTmp = NULL;
					newStrEx("cat ", &szTmp);
					appendStringBuf(pSb, szTmp);
					appendStringBuf(pSb, pSource->m_szName);
				}
				freeStr(&szTmp);
				szTmp = malloc(strlen(" > patch -p1234567890 -s ")+strlen(pSource->m_szName)+1);
				sprintf(szTmp, " | patch -p%d -s", nLevel);
				appendStringBuf(pSb, szTmp);
				appendLineStringBuf(pSb, "");
				appendLineStringBuf(pSb, "STATUS=$?");
				appendLineStringBuf(pSb, "if [ $STATUS -ne 0 ]; then");
				appendLineStringBuf(pSb, "    exit $STATUS");
				appendLineStringBuf(pSb, "fi");
				appendLineStringBuf(pSb, "");
				newStr(getStringBuf(pSb), (char**)&(pScript->m_szEntry));
				freeStringBuf(pSb);
				freeStr(&szTmp);
				freeStr(&szPath);
			}
			else {
				freeStr(&szPath);
				fprintf(stderr, "error: %s(%d): Invalid \"patch\" script. Patch not available.\n",
					pParse->m_szXMLFile,
					XML_GetCurrentLineNumber(pParse->m_pParser));
				pParse->m_nErrors++;
			}
			break;
		case TAGVAL_SUMMARY:
		case TAGVAL_DESCRIPTION:
		case TAGVAL_REQUIRES:
			// we don't need to do anything
			break;
		default:
			fprintf(stderr, "warning: %s(%d): Ignoring tag \"%s\" on level %d. (Function not implemented)\n",
				pParse->m_szXMLFile,
				XML_GetCurrentLineNumber(pParse->m_pParser),
				g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_szName,
				pParse->m_nLevel);
			pParse->m_nWarnings++;
			break;
	}
}

void handleXMLStartLevel3(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;
	t_structXMLPackage* pPkg = NULL;
	t_structXMLChanges* pChanges = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_REQUIRE:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			addXMLRequire(pParse->m_pAttrs, &(pPkg->m_pRequires));
			break;
		case TAGVAL_CHANGE:
			pChanges = getLastXMLChanges(pSpec->m_pChangelog);
			addXMLChange(pParse->m_pAttrs, &(pChanges->m_pChanges));
			break;
		case TAGVAL_SCRIPT: // pre, post, ...
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel-1]].m_nValue) {
				case TAGVAL_PRE:
					addXMLScript(pParse->m_pAttrs,
						     &(pPkg->m_pPre->m_pScripts));
					break;
				case TAGVAL_POST:
					addXMLScript(pParse->m_pAttrs,
						     &(pPkg->m_pPost->m_pScripts));
					break;
				case TAGVAL_PREUN:
					addXMLScript(pParse->m_pAttrs,
						     &(pPkg->m_pPreUn->m_pScripts));
					break;
				case TAGVAL_POSTUN:
					addXMLScript(pParse->m_pAttrs,
						     &(pPkg->m_pPostUn->m_pScripts));
					break;
				case TAGVAL_VERIFY:
					addXMLScript(pParse->m_pAttrs,
						     &(pPkg->m_pVerify->m_pScripts));
					break;
				default:
					break;
			}
			break;
		default:
			fprintf(stderr, "warning: %s(%d): Ignoring tag \"%s\" on level %d. (Function not implemented)\n",
				pParse->m_szXMLFile,
				XML_GetCurrentLineNumber(pParse->m_pParser),
				g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_szName,
				pParse->m_nLevel);
			pParse->m_nWarnings++;
			break;
	}
}

void startXMLCB(void* pData,
		const XML_Char* szName,
		const XML_Char** szaAttrs)
{
	t_structXMLParse* pParse = NULL;
	int nTagPos;

	if (!(pParse = (t_structXMLParse*)pData))
		return;

	if ((nTagPos = verifyXMLPath(szName, pParse)) == -1) {
		pParse->m_nLevel++;
		return;
	}

	if (verifyXMLAttrs(szaAttrs, pParse, nTagPos) == -1) {
		pParse->m_nLevel++;
		return;
	}

	pParse->m_naTree[pParse->m_nLevel] = nTagPos;
	switch (pParse->m_nLevel) {
		case 0:
			handleXMLStartLevel0(pParse);
			break;
		case 1:
			handleXMLStartLevel1(pParse);
			break;
		case 2:
			handleXMLStartLevel2(pParse);
			break;
		case 3:
			handleXMLStartLevel3(pParse);
			break;
		default:
			fprintf(stderr, "warning: %s(%d): XML tag nesting of %d levels not handled Ignoring tag \"%s\".\n",
				pParse->m_szXMLFile,
				XML_GetCurrentLineNumber(pParse->m_pParser),
				pParse->m_nLevel,
				szName);
			pParse->m_nWarnings++;
			break;
	}

	pParse->m_nLevel++;
	pParse->m_nLastGoodLevel = pParse->m_nLevel;
	return;
}

void handleXMLEndLevel0(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		default:
			break;
	}
}

void handleXMLEndLevel1(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;
	t_structXMLMacro* pMacro = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_MACRO:
			pMacro = getLastXMLMacro(pSpec->m_pMacros);
			newStr(pParse->m_szValue, &(pMacro->m_szValue));
			break;
		default:
			break;
	}
}

void handleXMLEndLevel2(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;
	t_structXMLPackage* pPkg = NULL;
	t_structXMLScript* pScript = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_SUMMARY:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			newStrEx(pParse->m_szValue, &(pPkg->m_szSummary));
			break;
		case TAGVAL_DESCRIPTION:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			newStrEx(pParse->m_szValue, &(pPkg->m_szDescription));
			break;
		case TAGVAL_SCRIPT:
			switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel-1]].m_nValue) {
				case TAGVAL_PREP:
					pScript = getLastXMLScript(pSpec->m_pPrep->m_pScripts);
					break;
				case TAGVAL_BUILD:
					pScript = getLastXMLScript(pSpec->m_pBuild->m_pScripts);
					break;
				case TAGVAL_INSTALL:
					pScript = getLastXMLScript(pSpec->m_pInstall->m_pScripts);
					break;
				case TAGVAL_CLEAN:
					pScript = getLastXMLScript(pSpec->m_pClean->m_pScripts);
					break;
				default:
					break;
			}
			if (pScript)
				newStrEx(pParse->m_szValue, (char**)&(pScript->m_szEntry));
			break;
		default:
			break;
	}
}

void handleXMLEndLevel3(t_structXMLParse* pParse)
{
	t_structXMLSpec* pSpec = NULL;
	t_structXMLPackage* pPkg = NULL;
	t_structXMLScript* pScript = NULL;

	pSpec = pParse->m_pSpec;
	switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel]].m_nValue) {
		case TAGVAL_SCRIPT:
			pPkg = getLastXMLPackage(pSpec->m_pPackages);
			switch (g_pXMLTags[pParse->m_naTree[pParse->m_nLevel-1]].m_nValue) {
				case TAGVAL_PRE:
					pScript = getLastXMLScript(pPkg->m_pPre->m_pScripts);
					break;
				case TAGVAL_POST:
					pScript = getLastXMLScript(pPkg->m_pPost->m_pScripts);
					break;
				case TAGVAL_PREUN:
					pScript = getLastXMLScript(pPkg->m_pPreUn->m_pScripts);
					break;
				case TAGVAL_POSTUN:
					pScript = getLastXMLScript(pPkg->m_pPostUn->m_pScripts);
					break;
				case TAGVAL_VERIFY:
					pScript = getLastXMLScript(pPkg->m_pVerify->m_pScripts);
					break;
				default:
					break;
			}
			if (pScript)
				newStrEx(pParse->m_szValue,
					 (char**)&(pScript->m_szEntry));
			break;
		default:
			break;
	}
}

void endXMLCB(void* pData,
	      const XML_Char* szName)
{
	t_structXMLParse* pParse = NULL;

	if ((pParse = (t_structXMLParse*)pData)) {
		pParse->m_nLevel--;
		switch (pParse->m_nLevel) {
			case 0:
				handleXMLEndLevel0(pParse);
				break;
			case 1:
				handleXMLEndLevel1(pParse);
				break;
			case 2:
				handleXMLEndLevel2(pParse);
				break;
			case 3:
				handleXMLEndLevel3(pParse);
				break;
			default:
				break;
		}
		freeStr(&(pParse->m_szValue));
	}
}

void dataXMLCB(void* pData,
	       const XML_Char* szValue,
	       int nLen)
{
	t_structXMLParse* pParse = NULL;
	char* szTmp = NULL;
	char* szTmpValue = NULL;
	char* szPos = NULL;

	if ((pParse = (t_structXMLParse*)pData)) {
		szTmp = malloc(nLen+2);
		szTmp[nLen] = '\0';
		snprintf(szTmp, nLen+1, "%s", szValue);
		szPos = szTmp;
		while ((*szPos == ' ') ||
		       (*szPos == '\t') ||
		       (*szPos == '\r') ||
		       (*szPos == '\n'))
			szPos++;

		if (strlen(szPos)) {
			if (pParse->m_szValue) {
				szTmpValue = malloc(strlen(szPos)+strlen(pParse->m_szValue)+1);
				sprintf(szTmpValue, "%s%s", pParse->m_szValue, szPos);
				newStr(szTmpValue, &(pParse->m_szValue));
				free(szTmpValue);
			}
			else
				newStr(szPos, &(pParse->m_szValue));
		}
		free(szTmp);
	}
}

t_structXMLSpec* parseXMLSpec(const char* szFile, int nVerbose)
{
	XML_Parser pParser = NULL;
	t_structXMLParse* pSParse = NULL;
	t_structXMLSpec* pSpec = NULL;
	FILE* fIn = NULL;
	int nLen = 0;
	char szBuffer[1024+1];

	pSParse = malloc(sizeof(t_structXMLParse));
	pSParse->m_szXMLFile = (char*)szFile;
	pSParse->m_szValue = NULL;
	pSParse->m_nLevel = 0;
	pSParse->m_nLastGoodLevel = 0;
	pSParse->m_nWarnings = 0;
	pSParse->m_nErrors = 0;
	pSParse->m_pSpec = NULL;
	pSParse->m_pAttrs = NULL;
	pSParse->m_nVerbose = nVerbose;

	if ((fIn = fopen(szFile, "r"))) {
		if ((pParser = XML_ParserCreate(NULL))) {
			pSParse->m_pParser = pParser;
			XML_SetStartElementHandler(pParser, &startXMLCB);
			XML_SetEndElementHandler(pParser, &endXMLCB);
			XML_SetCharacterDataHandler(pParser, &dataXMLCB);
			XML_SetUserData(pParser, pSParse);
			while (!feof(fIn)) {
				nLen = fread(szBuffer, sizeof(char), 1024, fIn);
				if (!XML_Parse(pParser, szBuffer, nLen, 0)) {
					fprintf(stderr, "fatal: %s(%d): %s \n", szFile,
							XML_GetCurrentLineNumber(pParser),
							XML_ErrorString(XML_GetErrorCode(pParser)));
					pSParse->m_nErrors++;
				}
			}
			if (nVerbose)
				printf("+ XML parse completed with %d error(s), %d warning(s).\n", pSParse->m_nErrors, pSParse->m_nWarnings);
			XML_Parse(pParser, szBuffer, 0, 1);
			XML_ParserFree(pParser);
		}
		fclose(fIn);
	}
	else {
		free(pSParse);
		fprintf(stderr, "fatal: Open of \"%s\" failed.\n", szFile);
		return NULL;
	}

	freeXMLAttr(&(pSParse->m_pAttrs));
	if (pSParse->m_nErrors)
		freeXMLSpec(&(pSParse->m_pSpec));

	pSpec = pSParse->m_pSpec;
	free(pSParse);
	return pSpec;
}
