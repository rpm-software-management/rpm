#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpmio.h"
#include "header.h"
#include "stringbuf.h"
#include "rpmspec.h"
#include "rpmbuild.h"
#include "header_internal.h"

#include "xml2rpm.h"
#include "xmlstruct.h"

// This is where our packaged scripts start (or the largest number
// of the sources)
int g_nMaxSourceNum = 511;

static struct ReqComp {
	const char*   m_szCmp;
	rpmsenseFlags m_rpmCmp;
} sReqComp[] = {
		{"le", RPMSENSE_LESS | RPMSENSE_EQUAL   },
		{"lt", RPMSENSE_LESS                    },
		{"eq", RPMSENSE_EQUAL                   },
		{"ge", RPMSENSE_GREATER | RPMSENSE_EQUAL},
		{"gt", RPMSENSE_GREATER                 },
		{NULL, 0                                }
	       };

Spec        g_pSpec     = NULL;
const char* g_szDefLang = "C";

#ifdef XML_DEBUG
void printSourceInfo(struct Source* pSource)
{
	if (pSource) {
		printf("D:     Source                     (%08x){\n", (unsigned int)pSource);
		printf("D:         Source->fullSource     (%08x): {%s}\n", (unsigned int)(pSource->fullSource), pSource->fullSource);
		printf("D:         Source->source         (%08x): {%s}\n", (unsigned int)(pSource->source), pSource->source);
		printf("D:         Source->flags                    : {%08x}\n", pSource->flags);
		printf("D:         Source->num                      : {%d}\n", pSource->num);
		printf("D:     }\n");
	}
}
#endif

#ifdef XML_DEBUG
void printHeaderInfo(Header pHeader)
{
	int i = 0;
	int j = 0;
	int nVal = 0;
	indexEntry pIdx;
	const struct headerTagTableEntry_s* pEntry;
	char* pData;
	int nCnt;
	char szType[20];

	if (pHeader) {
		printf("D:             Header->indexUsed            : {%d}\n", pHeader->indexUsed);
		printf("D:             Header->indexAlloced         : {%d}\n", pHeader->indexAlloced);
		printf("D:             Header->flags                : {%d}\n", pHeader->flags);
		printf("D:             Header->nrefs                : {%d}\n", pHeader->nrefs);
		printf("D:             Header->hv                   {\n");
		printf("D:                 HV.hdrvecs               : {%08x}\n", (unsigned int)(pHeader->hv.hdrvecs));
		printf("D:                 HV.hdrdata               : {%08x}\n", (unsigned int)(pHeader->hv.hdrdata));
		printf("D:                 HV.hdrversion            : {%d}\n", pHeader->hv.hdrversion);
		printf("D:             }\n");
		printf("D:             Header->blob       (%08x): {\n", (unsigned int)(pHeader->blob));
		pIdx = pHeader->index;
		for (i = 0; i < pHeader->indexUsed; i++, pIdx++) {
			pEntry = rpmTagTable;
			while (pEntry->name && pEntry->val != pIdx->info.tag)
				pEntry++;
			nCnt = pIdx->info.count;
			pData = pIdx->data;
			switch (pIdx->info.type) {
				case RPM_STRING_TYPE:       sprintf(szType, "STRING"); break;
	    			case RPM_STRING_ARRAY_TYPE: sprintf(szType, "STRING_ARRAY"); break;
	    			case RPM_I18NSTRING_TYPE:   sprintf(szType, "STRING_I18N"); break;
				case RPM_CHAR_TYPE:         sprintf(szType, "CHAR"); break;
				case RPM_INT8_TYPE:         sprintf(szType, "INT8"); break;
				case RPM_INT16_TYPE:        sprintf(szType, "INT16"); break;
				case RPM_INT32_TYPE:        sprintf(szType, "INT32"); break;
				default:                    sprintf(szType, "OTHER"); break;
			}
			printf("D:                 %02d %s(%02d) %s\n", i, pEntry->name, nCnt, szType);
			switch (pIdx->info.type) {
				case RPM_STRING_TYPE:
	    			case RPM_STRING_ARRAY_TYPE:
	    			case RPM_I18NSTRING_TYPE:
					for (j = 0; j < nCnt; j++) {
						if (strlen(pData))
							printf("D:                    %02d {%s}\n", j, pData);
						pData = strchr(pData, 0);
						pData++;
					}
					break;
				case RPM_CHAR_TYPE:
				case RPM_INT8_TYPE:
				case RPM_INT16_TYPE:
				case RPM_INT32_TYPE:
					for (j = 0; j < nCnt; j++) {
						switch (pIdx->info.type) {
							case RPM_CHAR_TYPE:
							case RPM_INT8_TYPE:
								nVal = *(((int_8*)pData)+j);
								break;
							case RPM_INT16_TYPE:
								nVal = *(((uint_16*)pData)+j);
								break;
							case RPM_INT32_TYPE:
							default:
								nVal = *(((int_32*)pData)+j);
								break;
						}
						printf("D:                    %02d {%08x}\n", j, nVal);
					}
					break;
				default:
					break;
			}
		}
		printf("D:             }\n");
	}
}
#endif

#ifdef XML_DEBUG
void printMacroInfo(MacroContext pMacro)
{
	int i = 0;
	MacroEntry pEntry = NULL;

	if (pMacro) {
		for (i = 0; i < pMacro->firstFree; i++) {
			if ((pEntry = pMacro->macroTable[i]) &&
			    (pEntry->level >= RMIL_SPEC)) {
				printf("D:         %03d % 3d %s", i, pEntry->level, 
								     pEntry->name);
				if ((pEntry->opts) &&
				    (*pEntry->opts))
					printf("(%s)", pEntry->opts);
				printf("\n");
				if ((pEntry->level >= RMIL_SPEC) &&
				    (pEntry->body) &&
				    (*pEntry->body))
					printf("D:                 {%s}\n", pEntry->body);
			}
		}
	}
}
#endif

#ifdef XML_DEBUG
void printSpecInfo(Spec pSpec)
{
	struct Source* pSource = NULL;
	struct PackageStruct* pPackage = NULL;

	printf("D: Spec(%08x)                           {\n", (unsigned int)pSpec);
	if (pSpec) {
		printf("D:     Spec->specfile             (%08x): {%s}\n", (unsigned int)(pSpec->specFile), pSpec->specFile);
		printf("D:     Spec->sourceRpmName        (%08x): {%s}\n", (unsigned int)(pSpec->sourceRpmName), pSpec->sourceRpmName);
		printf("D:     Spec->buildRootURL         (%08x): {%s}\n", (unsigned int)(pSpec->buildRootURL), pSpec->buildRootURL);
		printf("D:     Spec->buildSubdir          (%08x): {%s}\n", (unsigned int)(pSpec->buildSubdir), pSpec->buildSubdir);
		printf("D:     Spec->rootURL              (%08x): {%s}\n", (unsigned int)(pSpec->rootURL), pSpec->rootURL);
		printf("D:     Spec->BACount                        : {%d}\n", pSpec->BACount);
		printf("D:     Spec->recursing                      : {%d}\n", pSpec->recursing);
		printf("D:     Spec->force                          : {%d}\n", pSpec->force);
		printf("D:     Spec->anyarch                        : {%d}\n", pSpec->anyarch);
		printf("D:     Spec->gotBuildRootURL                : {%d}\n", pSpec->gotBuildRootURL);
		printf("D:     Spec->timeCheck                      : {%d}\n", pSpec->timeCheck);
		printf("D:     Spec->passPhrase           (%08x): {%s}\n", (unsigned int)(pSpec->passPhrase), pSpec->passPhrase);
		printf("D:     Spec->cookie               (%08x): {%s}\n", (unsigned int)(pSpec->cookie), pSpec->cookie);
		printf("D:     Spec->numSources                     : {%d}\n", pSpec->numSources);
		printf("D:     Spec->noSource                       : {%d}\n", pSpec->noSource);
		printf("D:     Spec->sources                        : (%08x)\n", (unsigned int)(pSpec->sources));
		pSource = pSpec->sources;
		while (pSource) {
			printSourceInfo(pSource);
			pSource = pSource->next;
		}
		printf("D:     Spec->buildRestrictions    (%08x){\n", (unsigned int)(pSpec->buildRestrictions));
		printHeaderInfo(pSpec->buildRestrictions);
		printf("D:     }\n");
		printf("D:     Spec->sourceHeader         (%08x){\n", (unsigned int)(pSpec->sourceHeader));
		printHeaderInfo(pSpec->sourceHeader);
		printf("D:     }\n");
		printf("D:     Spec->packages                       : {%08x}\n", (unsigned int)(pSpec->packages));
		pPackage = pSpec->packages;
		while (pPackage) {
			printf("D:     Package(%08x)                    {\n", (unsigned int)pPackage);
			printf("D:         Package->cpioList                : {%08x}\n", (unsigned int)(pPackage->cpioList));
			printf("D:         Package->icon                    : {%08x}\n", (unsigned int)(pPackage->icon));
			printf("D:         Package->autoReq                 : {%d}\n", pPackage->autoReq);
			printf("D:         Package->autoProv                : {%d}\n", pPackage->autoProv);
			printf("D:         Package->preInFile     (%08x): {%s}\n", (unsigned int)(pPackage->preInFile), pPackage->preInFile);
			printf("D:         Package->postInFile    (%08x): {%s}\n", (unsigned int)(pPackage->postInFile), pPackage->postInFile);
			printf("D:         Package->preUnFile     (%08x): {%s}\n", (unsigned int)(pPackage->preUnFile), pPackage->preUnFile);
			printf("D:         Package->postUnFile    (%08x): {%s}\n", (unsigned int)(pPackage->postUnFile), pPackage->postUnFile);
			printf("D:         Package->triggerFiles            : {%08x}\n", (unsigned int)(pPackage->triggerFiles));
			printf("D:         Package->fileFile      (%08x): {%s}\n", (unsigned int)(pPackage->fileFile), pPackage->fileFile);
			printf("D:         Package->fileFile      (%08x): {%s}\n", (unsigned int)(pPackage->fileList), pPackage->fileList ? getStringBuf(pPackage->fileList) : NULL);
			printf("D:         Package->header        (%08x){\n", (unsigned int)(pPackage->header));
			printHeaderInfo(pPackage->header);
			printf("D:         }\n");
			pPackage = pPackage->next;
			printf("D:     }\n");
		}
		printf("D:     Spec->macros               (%08x){\n", (unsigned int)(pSpec->macros));
		printMacroInfo(pSpec->macros);
		printf("D:     }\n");
		printf("D:     Spec->prep                 (%08x): {%s}\n", (unsigned int)(pSpec->prep), pSpec->prep ? getStringBuf(pSpec->prep) : NULL);
		printf("D:     Spec->build                (%08x): {%s}\n", (unsigned int)(pSpec->build), pSpec->build ? getStringBuf(pSpec->build) : NULL);
		printf("D:     Spec->install              (%08x): {%s}\n", (unsigned int)(pSpec->install), pSpec->install ? getStringBuf(pSpec->install) : NULL);
		printf("D:     Spec->clean                (%08x): {%s}\n", (unsigned int)(pSpec->clean), pSpec->clean ? getStringBuf(pSpec->clean) : NULL);
	}
	printf("D: }\n");
}
#endif

void createRPMSource(const char* szName, int nNum,
		     Spec pSpec, int nType)
{
	struct Source* pSrc = NULL;
	struct Source* pPrev = NULL;
	char szTmpName[20];
	char szPre[3];
	char szType[7];

	if (!szName || !pSpec)
		return;

	if ((nType & RPMBUILD_ISNO) == RPMBUILD_ISNO) {
		pSpec->noSource++;
		sprintf(szPre, "NO");
	}
	else {
		pSpec->numSources++;
		szPre[0] = '\0';
	}
	pSrc = malloc(sizeof(struct Source));
	pSrc->fullSource = NULL;
	newStrEx(szName, &(pSrc->fullSource));
	if (!(pSrc->source = strrchr(pSrc->fullSource, '/')))
		pSrc->source = pSrc->fullSource;
	else
		pSrc->source++;

	pSrc->num = nNum;
	pSrc->flags = nType;
	pSrc->next = NULL;
	if ((pPrev = pSpec->sources)) {
		while (pPrev->next)
			pPrev = pPrev->next;
		pPrev->next = pSrc;
	}
	else
		pSpec->sources = pSrc;

	if ((nType & RPMBUILD_ISSOURCE) == RPMBUILD_ISSOURCE)
		sprintf(szType, "SOURCE");
	else
		sprintf(szType, "PATCH");
	sprintf(szTmpName, "%s%s%d", szPre, szType, nNum);
	addMacro(pSpec->macros, szTmpName, NULL, pSrc->fullSource, RMIL_SPEC);
	sprintf(szTmpName, "%s%sURL%d", szPre, szType, nNum);
	addMacro(pSpec->macros, szTmpName, NULL, pSrc->source, RMIL_SPEC);

	if (nNum > g_nMaxSourceNum)
		g_nMaxSourceNum = nNum;
}

void convertXMLSource(const t_structXMLSource* pXMLSrc,
		      Spec pSpec, int nType)
{
	if (!pXMLSrc || !pSpec)
		return;

	createRPMSource(pXMLSrc->m_szName, pXMLSrc->m_nNum, pSpec, nType);
	convertXMLSource(pXMLSrc->m_pNext, pSpec, nType);
}

StringBuf scriptsToStringBuf(const t_structXMLScripts* pScripts,
			     Spec pSpec)
{
	StringBuf pSb = NULL;
	t_structXMLScript* pScript = NULL;
	char* szTmp = NULL;

	if (pScripts) {
		pSb = newStringBuf();
		if ((pScripts->m_szScript) &&
		    (szTmp = fileToStr(pScripts->m_szScript, NULL))) {
			appendLineStringBuf(pSb, szTmp);
			freeStr(&(szTmp));
			createRPMSource(pScripts->m_szScript, g_nMaxSourceNum+1,
					pSpec, RPMBUILD_ISSOURCE);
		}
		pScript = pScripts->m_pScripts;
		while (pScript) {
			if ((pScript->m_szScript) &&
			    (szTmp = fileToStr(pScript->m_szScript, NULL))) {
				appendLineStringBuf(pSb, szTmp);
				freeStr(&(szTmp));
				createRPMSource(pScript->m_szScript, g_nMaxSourceNum+1,
						pSpec, RPMBUILD_ISSOURCE);
			}
			if (pScript->m_szEntry)
				appendLineStringBuf(pSb, pScript->m_szEntry);
			pScript = pScript->m_pNext;
		}
	}

	return pSb;
}

void convertXMLScripts(const t_structXMLScripts* pScripts,
		       struct PackageStruct* pPkg,
		       Spec pSpec,
		       int nScript, int nInterpreter)
{
	StringBuf pSb = NULL;

	if (!pScripts || !pPkg)
		return;

	if ((pSb = scriptsToStringBuf(pScripts, pSpec))) {
		headerAddEntry(pPkg->header,
			       nScript, RPM_STRING_TYPE,
			       getStringBuf(pSb), 1);
		freeStringBuf(pSb);
	}
	if (pScripts->m_szInterpreter)
		headerAddEntry(pPkg->header,
			       nInterpreter, RPM_STRING_TYPE,
			       pScripts->m_szInterpreter, 1);
}

void convertXMLPackage(const t_structXMLPackage* pXMLPkg,
		       const t_structXMLSpec* pXMLSpec,
		       Spec pSpec)
{
	struct PackageStruct* pPkg = NULL;
	char* szPlatform = NULL;
	char* szArch = NULL;
	char* szOs = NULL;
	char* szTmp = NULL;

	if (!pXMLPkg || !pSpec)
		return;

	pPkg = newPackage(pSpec);
	pPkg->autoReq = pXMLPkg->m_nAutoRequire;
	pPkg->autoProv = pXMLPkg->m_nAutoProvide;
	pPkg->header = headerNew();

	szPlatform = rpmExpand("%{_target_platform}", NULL);
	szArch = rpmExpand("%{_target_cpu}", NULL);
	szOs = rpmExpand("%{_target_os}", NULL);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_OS, RPM_STRING_TYPE, szOs, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_ARCH, RPM_STRING_TYPE, szArch, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_RHNPLATFORM, RPM_STRING_TYPE, szArch, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_PLATFORM, RPM_STRING_TYPE, szPlatform, 1);
	_free(szPlatform);
	_free(szArch);
	_free(szOs);

	if (pXMLPkg->m_szName)
		headerAddOrAppendEntry(pPkg->header, RPMTAG_NAME,
				       RPM_STRING_TYPE, pXMLPkg->m_szName, 1);
	else
		headerAddOrAppendEntry(pPkg->header, RPMTAG_NAME,
				       RPM_STRING_TYPE, pXMLSpec->m_szName, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_VERSION,
			       RPM_STRING_TYPE, pXMLSpec->m_szVersion, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_RELEASE,
			       RPM_STRING_TYPE, pXMLSpec->m_szRelease, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_GROUP,
			       RPM_STRING_TYPE, pXMLPkg->m_szGroup, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_RELEASE,
			       RPM_STRING_TYPE, pXMLSpec->m_szRelease, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_LICENSE,
			       RPM_STRING_TYPE, pXMLSpec->m_szCopyright, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_URL,
			       RPM_STRING_TYPE, pXMLSpec->m_szURL, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_DISTRIBUTION,
			       RPM_STRING_TYPE, pXMLSpec->m_szDistribution, 1);
	headerAddOrAppendEntry(pPkg->header, RPMTAG_VENDOR,
			       RPM_STRING_TYPE, pXMLSpec->m_szVendor, 1);
	headerAddOrAppendEntry(pPkg->header, HEADER_I18NTABLE,
			       RPM_STRING_ARRAY_TYPE, &g_szDefLang, 1);
	headerAddI18NString(pPkg->header, RPMTAG_SUMMARY,
			    pXMLPkg->m_szSummary, g_szDefLang);
	headerAddI18NString(pPkg->header, RPMTAG_DESCRIPTION,
			    pXMLPkg->m_szSummary, g_szDefLang);

	if (pXMLSpec->m_szPackagerEmail || pXMLSpec->m_szPackager) {
		szTmp = malloc(strlen(pXMLSpec->m_szPackager)+strlen(pXMLSpec->m_szPackagerEmail)+4);
		if (pXMLSpec->m_szPackagerEmail && pXMLSpec->m_szPackager)
			sprintf(szTmp, "%s <%s>", pXMLSpec->m_szPackager,
						  pXMLSpec->m_szPackagerEmail);
		else
			sprintf(szTmp, "%s", pXMLSpec->m_szPackager ? pXMLSpec->m_szPackager : pXMLSpec->m_szPackagerEmail);
		headerAddOrAppendEntry(pPkg->header, RPMTAG_PACKAGER,
				       RPM_STRING_TYPE, szTmp, 1);
		free(szTmp);
	}

	if (pXMLPkg->m_pFiles)
		pPkg->fileList = fileToStrBuf(pXMLPkg->m_pFiles->m_szFileList, NULL);

	convertXMLScripts(pXMLPkg->m_pPre, pPkg, pSpec,
			  RPMTAG_PREIN, RPMTAG_PREINPROG);
	convertXMLScripts(pXMLPkg->m_pPost, pPkg, pSpec,
			  RPMTAG_POSTIN, RPMTAG_POSTINPROG);
	convertXMLScripts(pXMLPkg->m_pPreUn, pPkg, pSpec,
			  RPMTAG_PREUN, RPMTAG_PREUNPROG);
	convertXMLScripts(pXMLPkg->m_pPostUn, pPkg, pSpec,
			  RPMTAG_POSTUN, RPMTAG_POSTUNPROG);
	convertXMLScripts(pXMLPkg->m_pVerify, pPkg, pSpec,
			  RPMTAG_VERIFYSCRIPT, RPMTAG_VERIFYSCRIPTPROG);

	convertXMLPackage(pXMLPkg->m_pNext, pXMLSpec, pSpec);
}

void convertXMLMacro(const t_structXMLMacro* pMacro,
		     Spec pSpec)
{
	if (!pMacro || !pSpec)
		return;

	addMacroEx(pMacro->m_szName, (char**)&(pMacro->m_szValue), 1);
	convertXMLMacro(pMacro->m_pNext, pSpec);
}

void convertXMLBuildRequires(const t_structXMLSpec* pXMLSpec,
			     Spec pSpec)
{
	t_structXMLRequire* pRequire = NULL;
	char* szTmp = NULL;
	char* szVal = NULL;
	int i = 0;
	int nFlags = 0;
	int nIndex = 0;
	char* szVersion = NULL;

	pRequire = pXMLSpec->m_pBuildRequires;
	szVal = malloc(1);
	szVal[0] = '\0';
	while (pRequire) {
		if (pRequire->m_szName) {
			szTmp = malloc(strlen(szVal)+strlen(pRequire->m_szName)+2);
			if (strlen(szVal))
				sprintf(szTmp, "%s", pRequire->m_szName);
			else
				sprintf(szTmp, "%s %s", szVal, pRequire->m_szName);
			free(szVal);
			szVal = szTmp;
		}
		szVersion = NULL;
		nFlags = 0;
		if (pRequire->m_szCompare && pRequire->m_szVersion) {
			for (i = 0; sReqComp[i].m_szCmp; i++)
				if (!strcasecmp(sReqComp[i].m_szCmp, pRequire->m_szCompare)) {
					nFlags = (sReqComp[i].m_rpmCmp | RPMSENSE_ANY) & ~RPMSENSE_SENSEMASK;
					szVersion = pRequire->m_szVersion;
				}
		}
		addReqProv(pSpec, pSpec->buildRestrictions, nFlags,
			   pRequire->m_szName, pRequire->m_szVersion, nIndex);
		addReqProv(pSpec, pSpec->sourceHeader, nFlags,
			   pRequire->m_szName, pRequire->m_szVersion, nIndex);
		pRequire = pRequire->m_pNext;
		nIndex++;
	}
	if (pXMLSpec->m_szPackagerEmail || pXMLSpec->m_szPackager) {
		szTmp = malloc(strlen(pXMLSpec->m_szPackager)+strlen(pXMLSpec->m_szPackagerEmail)+4);
		if (pXMLSpec->m_szPackagerEmail && pXMLSpec->m_szPackager)
			sprintf(szTmp, "%s <%s>", pXMLSpec->m_szPackager, pXMLSpec->m_szPackagerEmail);
		else
			sprintf(szTmp, "%s", pXMLSpec->m_szPackager ? pXMLSpec->m_szPackager : pXMLSpec->m_szPackagerEmail);
		headerAddOrAppendEntry(pSpec->packages->header, RPMTAG_PACKAGER,
				       RPM_STRING_TYPE, szTmp, 1);
		free(szTmp);
	}
	free(szVal);
}

Spec toRPMStruct(const t_structXMLSpec* pXMLSpec)
{
	Spec pSpec = NULL;
	char* szTmp = NULL;

	if (!pXMLSpec)
		return NULL;

	pSpec = g_pSpec;
	newStrEx(pXMLSpec->m_szSpecName, (char**)&(pSpec->specFile));
	if (pXMLSpec->m_pPackages) {
		addMacroEx("group",
			   (char**)&(pXMLSpec->m_pPackages->m_szGroup), RMIL_SPEC);
		addMacroEx("summary",
			   (char**)&(pXMLSpec->m_pPackages->m_szSummary), RMIL_SPEC);
	}
	pSpec->gotBuildRootURL = 1;

	newStrEx(pXMLSpec->m_szBuildRootDir, (char**)(&(pSpec->buildRootURL)));
	newStrEx(pXMLSpec->m_szBuildSubdir, (char**)(&(pSpec->buildSubdir)));
	newStrEx(pXMLSpec->m_szRootDir, (char**)(&(pSpec->rootURL)));

	convertXMLSource(pXMLSpec->m_pSources, pSpec, RPMBUILD_ISSOURCE);
	convertXMLSource(pXMLSpec->m_pPatches, pSpec, RPMBUILD_ISPATCH);

	pSpec->prep = scriptsToStringBuf(pXMLSpec->m_pPrep, pSpec);
	pSpec->build = scriptsToStringBuf(pXMLSpec->m_pBuild, pSpec);
	pSpec->install = scriptsToStringBuf(pXMLSpec->m_pInstall, pSpec);
	pSpec->clean = scriptsToStringBuf(pXMLSpec->m_pClean, pSpec);
	
	convertXMLPackage(pXMLSpec->m_pPackages, pXMLSpec, pSpec);

	initSourceHeader(pSpec);
	if (pXMLSpec->m_szPackagerEmail || pXMLSpec->m_szPackager) {
		szTmp = malloc(strlen(pXMLSpec->m_szPackager)+strlen(pXMLSpec->m_szPackagerEmail)+4);
		if (pXMLSpec->m_szPackagerEmail && pXMLSpec->m_szPackager)
			sprintf(szTmp, "%s <%s>", pXMLSpec->m_szPackager,
						  pXMLSpec->m_szPackagerEmail);
		else
			sprintf(szTmp, "%s", pXMLSpec->m_szPackager ? pXMLSpec->m_szPackager : pXMLSpec->m_szPackagerEmail);
		headerAddOrAppendEntry(pSpec->sourceHeader, RPMTAG_PACKAGER,
				       RPM_STRING_TYPE, szTmp, 1);
		free(szTmp);
	}

	pSpec->buildRestrictions = headerNew();
	convertXMLBuildRequires(pXMLSpec, pSpec);

#ifdef XML_DEBUG
	printSpecInfo(pSpec);
#endif

	return pSpec;
}
