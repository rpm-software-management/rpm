#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "stringbuf.h"
#include "rpmbuild.h"

#include "xmlmisc.h"
#include "xmlstruct.h"
#include "xmlverify.h"

extern Spec g_pSpec;
char* g_szSpecPath = NULL;

void attrSetStr(const t_structXMLAttr* pAttr,
		const char* szParam,
		char** pszVar)
{
	if (pAttr && !strcasecmp(pAttr->m_szName, szParam))
		newStrEx(pAttr->m_szValue, pszVar);
}

void attrSetInt(const t_structXMLAttr* pAttr,
		const char* szParam,
		int* pnVar)
{
	if (pAttr && !strcasecmp(pAttr->m_szName, szParam))
		(*pnVar) = atoi(pAttr->m_szValue);
}

void attrSetBool(const t_structXMLAttr* pAttr,
		 const char* szParam,
		 int* pnVar)
{
	if (pAttr && !strcasecmp(pAttr->m_szName, szParam))
		(*pnVar) = strToBool(pAttr->m_szValue);
}

t_structXMLAttr* newXMLAttr(const char* szName,
			    const char* szValue,
			    int nType)
{
	t_structXMLAttr* pAttr = NULL;

	if ((pAttr = malloc(sizeof(t_structXMLAttr)))) {
		pAttr->m_szName = NULL;
		pAttr->m_szValue = NULL;
		pAttr->m_nType = nType;
		pAttr->m_pNext = NULL;

		newStrEx(szName, &(pAttr->m_szName));
		newStrEx(szValue, &(pAttr->m_szValue));
	}

	return pAttr;
}

int freeXMLAttr(t_structXMLAttr** ppAttr)
{
	if (*ppAttr) {
		freeStr(&((*ppAttr)->m_szName));
		freeStr(&((*ppAttr)->m_szValue));
		freeXMLAttr(&((*ppAttr)->m_pNext));
		free(*ppAttr);
		*ppAttr = NULL;
	}

	return 0;
}

t_structXMLAttr* addXMLAttr(const char* szName,
			    const char* szValue,
			    int nType,
			    t_structXMLAttr** ppAttr)
{
	t_structXMLAttr* pAttr = NULL;

	if ((pAttr = getLastXMLAttr(*ppAttr)))
		pAttr = (pAttr->m_pNext = newXMLAttr(szName, szValue, nType));
	else
		pAttr = (*ppAttr = newXMLAttr(szName, szValue, nType));

	return pAttr;
}

t_structXMLAttr* getLastXMLAttr(t_structXMLAttr* pAttr)
{
	while (pAttr && (pAttr->m_pNext))
		pAttr = pAttr->m_pNext;

	return pAttr;
}

t_structXMLAttr* getXMLAttr(const char* szName,
			    t_structXMLAttr* pAttr)
{
	while (pAttr && (strcasecmp(szName, pAttr->m_szName) != 0))
		pAttr = pAttr->m_pNext;
		
	return pAttr;
}

t_structXMLMacro* newXMLMacro(const t_structXMLAttr* pAttrs)
{
	t_structXMLMacro* pMacro = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pMacro = malloc(sizeof(t_structXMLMacro)))) {
		pMacro->m_szName = NULL;
		pMacro->m_szValue = NULL;
		pMacro->m_pNext = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "name", &(pMacro->m_szName));
		} while ((pAttr = pAttr->m_pNext));
		//addMacroEx(pMacro->m_szName, (char**)&(pMacro->m_szValue), 1);
	}

	return pMacro;
}

int freeXMLMacro(t_structXMLMacro** ppMacro)
{
	if (*ppMacro) {
		freeStr(&((*ppMacro)->m_szName));
		freeStr(&((*ppMacro)->m_szValue));
		freeXMLMacro(&((*ppMacro)->m_pNext));
		free(*ppMacro);
	}
	*ppMacro = NULL;

	return 0;
}

t_structXMLMacro* addXMLMacro(const t_structXMLAttr* pAttrs,
			      t_structXMLMacro** ppMacro)
{
	t_structXMLMacro* pMacro = NULL;

	if ((pMacro = getLastXMLMacro(*ppMacro)))
		pMacro = (pMacro->m_pNext = newXMLMacro(pAttrs));
	else
		pMacro = (*ppMacro = newXMLMacro(pAttrs));

	return pMacro;
}

t_structXMLMacro* getLastXMLMacro(t_structXMLMacro* pMacro)
{
	while (pMacro && (pMacro->m_pNext))
		pMacro = pMacro->m_pNext;

	return pMacro;
}

t_structXMLMirror* newXMLMirror(const t_structXMLAttr* pAttrs)
{
	t_structXMLMirror* pMirror = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pMirror = malloc(sizeof(t_structXMLMirror)))) {
		pMirror->m_szName = NULL;
		pMirror->m_szLocation = NULL;
		pMirror->m_szCountry = NULL;
		pMirror->m_szPath = NULL;
		pMirror->m_nTries = 0;
		pMirror->m_pNext = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "name", &(pMirror->m_szName));
			attrSetStr(pAttr, "location", &(pMirror->m_szLocation));
			attrSetStr(pAttr, "country", &(pMirror->m_szCountry));
			attrSetStr(pAttr, "path", &(pMirror->m_szPath));
		} while ((pAttr = pAttr->m_pNext));
	}

	return pMirror;
}

int freeXMLMirror(t_structXMLMirror** ppMirror)
{
	if (*ppMirror) {
		freeStr(&((*ppMirror)->m_szName));
		freeStr(&((*ppMirror)->m_szLocation));
		freeStr(&((*ppMirror)->m_szCountry));
		freeStr(&((*ppMirror)->m_szPath));
		freeXMLMirror(&((*ppMirror)->m_pNext));
		free(*ppMirror);
	}
	*ppMirror = NULL;

	return 0;
}

t_structXMLMirror* addXMLMirror(const t_structXMLAttr* pAttrs,
				t_structXMLMirror** ppMirror)
{
	t_structXMLMirror* pMirror = NULL;

	if ((pMirror = getLastXMLMirror(*ppMirror)))
		pMirror = (pMirror->m_pNext = newXMLMirror(pAttrs));
	else
		pMirror = (*ppMirror = newXMLMirror(pAttrs));

	return pMirror;
}

t_structXMLMirror* getLastXMLMirror(t_structXMLMirror* pMirror)
{
	while (pMirror && (pMirror->m_pNext))
		pMirror = pMirror->m_pNext;

	return pMirror;
}

t_structXMLSource* newXMLSource(const t_structXMLAttr* pAttrs)
{
	t_structXMLSource* pSource = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pSource = malloc(sizeof(t_structXMLSource)))) {
		pSource->m_szName = NULL;
		pSource->m_szMD5 = NULL;
		pSource->m_szDirectory = NULL;
		pSource->m_nSize = 0;
		pSource->m_nNum = -1;
		pSource->m_pMirrors = NULL;
		pSource->m_pNext = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "name", &(pSource->m_szName));
			attrSetInt(pAttr, "size", &(pSource->m_nSize));
			attrSetStr(pAttr, "md5", &(pSource->m_szMD5));
			attrSetStr(pAttr, "directory", &(pSource->m_szDirectory));
			attrSetInt(pAttr, "number", &(pSource->m_nNum));
		} while ((pAttr = pAttr->m_pNext));
	}

	return pSource;
}

int freeXMLSource(t_structXMLSource** ppSource)
{
	if (*ppSource) {
		freeStr(&((*ppSource)->m_szName));
		freeStr(&((*ppSource)->m_szMD5));
		freeStr(&((*ppSource)->m_szDirectory));
		freeXMLMirror(&((*ppSource)->m_pMirrors));
		freeXMLSource(&((*ppSource)->m_pNext));
		free(*ppSource);
	}
	*ppSource = NULL;

	return 0;
}

t_structXMLSource* addXMLSource(const t_structXMLAttr* pAttrs,
				t_structXMLSource** ppSource)
{
	t_structXMLSource* pSource = NULL;

	if ((pSource = getLastXMLSource(*ppSource))) {
		pSource->m_pNext = newXMLSource(pAttrs);
		if (pSource->m_pNext->m_nNum == -1)
			pSource->m_pNext->m_nNum = pSource->m_nNum + 1;
		pSource = pSource->m_pNext;
	}
	else {
		pSource = (*ppSource = newXMLSource(pAttrs));
		if (pSource->m_nNum == -1)
			pSource->m_nNum = 0;
	}

	return pSource;
}

t_structXMLSource* getLastXMLSource(t_structXMLSource* pSource)
{
	while (pSource && (pSource->m_pNext))
		pSource = pSource->m_pNext;

	return pSource;
}

t_structXMLSource* getXMLSource(int nNum,
				t_structXMLSource* pSource)
{
	while (pSource && (pSource->m_nNum != nNum))
		pSource = pSource->m_pNext;

	return pSource;
}

t_structXMLRequire* newXMLRequire(const t_structXMLAttr* pAttrs)
{
	t_structXMLRequire* pRequire = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pRequire = malloc(sizeof(t_structXMLRequire)))) {
		pRequire->m_szName = NULL;
		pRequire->m_szVersion = NULL;
		pRequire->m_szRelease = NULL;
		pRequire->m_szEpoch = NULL;
		pRequire->m_szCompare = NULL;
		pRequire->m_pNext = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "name", &(pRequire->m_szName));
			attrSetStr(pAttr, "version", &(pRequire->m_szVersion));
			attrSetStr(pAttr, "release", &(pRequire->m_szRelease));
			attrSetStr(pAttr, "epoch", &(pRequire->m_szEpoch));
			attrSetStr(pAttr, "compare", &(pRequire->m_szCompare));
		} while ((pAttr = pAttr->m_pNext));
	}

	return pRequire;
}

int freeXMLRequire(t_structXMLRequire** ppRequire)
{
	if (*ppRequire) {
		freeStr(&((*ppRequire)->m_szName));
		freeStr(&((*ppRequire)->m_szVersion));
		freeStr(&((*ppRequire)->m_szRelease));
		freeStr(&((*ppRequire)->m_szEpoch));
		freeStr(&((*ppRequire)->m_szCompare));
		freeXMLRequire(&((*ppRequire)->m_pNext));
		free(*ppRequire);
	}
	*ppRequire = NULL;

	return 0;
}

t_structXMLRequire* addXMLRequire(const t_structXMLAttr* pAttrs,
				  t_structXMLRequire** ppRequire)
{
	t_structXMLRequire* pRequire = NULL;

	if ((pRequire = getLastXMLRequire(*ppRequire)))
		pRequire = (pRequire->m_pNext = newXMLRequire(pAttrs));
	else
		pRequire = (*ppRequire = newXMLRequire(pAttrs));

	return pRequire;
}

t_structXMLRequire* getLastXMLRequire(t_structXMLRequire* pRequire)
{
	while (pRequire && (pRequire->m_pNext))
		pRequire = pRequire->m_pNext;

	return pRequire;
}

t_structXMLScript* newXMLScript(const t_structXMLAttr* pAttrs)
{
	t_structXMLScript* pScript = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pScript = malloc(sizeof(t_structXMLScript)))) {
		pScript->m_szInterpreter = NULL;
		pScript->m_szScript = NULL;
		pScript->m_szEntry = NULL;
		pScript->m_pNext = NULL;

		if ((pAttr = (t_structXMLAttr*)pAttrs)) {
			do {
				attrSetStr(pAttr, "interpreter",
					   &(pScript->m_szInterpreter));
				attrSetStr(pAttr, "script",
					   &(pScript->m_szScript));
			} while ((pAttr = pAttr->m_pNext));
		}
	}

	return pScript;
}

int freeXMLScript(t_structXMLScript** ppScript)
{
	if (*ppScript) {
		freeStr(&((*ppScript)->m_szInterpreter));
		freeStr(&((*ppScript)->m_szScript));
		freeStr(&((*ppScript)->m_szEntry));
		freeXMLScript(&((*ppScript)->m_pNext));
		free(*ppScript);
	}
	*ppScript = NULL;

	return 0;
}

t_structXMLScript* addXMLScript(const t_structXMLAttr* pAttrs,
				t_structXMLScript** ppScript)
{
	t_structXMLScript* pScript = NULL;

	if ((pScript = getLastXMLScript(*ppScript)))
		pScript = (pScript->m_pNext = newXMLScript(pAttrs));
	else
		pScript = (*ppScript = newXMLScript(pAttrs));

	return pScript;
}

t_structXMLScript* getLastXMLScript(t_structXMLScript* pScript)
{
	while (pScript && (pScript->m_pNext))
		pScript = pScript->m_pNext;

	return pScript;
}

t_structXMLScripts* newXMLScripts(const t_structXMLAttr* pAttrs)
{
	t_structXMLScripts* pScripts = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pScripts = malloc(sizeof(t_structXMLScripts)))) {
		pScripts->m_szInterpreter = NULL;
		pScripts->m_szScript = NULL;
		pScripts->m_pScripts = NULL;

		if ((pAttr = (t_structXMLAttr*)pAttrs)) {
			do {
				attrSetStr(pAttr, "interpreter", &(pScripts->m_szInterpreter));
				attrSetStr(pAttr, "script", &(pScripts->m_szScript));
			} while ((pAttr = pAttr->m_pNext));
		}
	}

	return pScripts;
}

int freeXMLScripts(t_structXMLScripts** ppScripts)
{
	if (*ppScripts) {
		freeStr(&((*ppScripts)->m_szInterpreter));
		freeStr(&((*ppScripts)->m_szScript));
		freeXMLScript(&((*ppScripts)->m_pScripts));
		free(*ppScripts);
	}
	*ppScripts = NULL;

	return 0;
}

t_structXMLFiles* newXMLFiles(const t_structXMLAttr* pAttrs)
{
	t_structXMLFiles* pFiles = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pFiles = malloc(sizeof(t_structXMLFiles)))) {
		pFiles->m_szFileList = NULL;
		pFiles->m_szUID = NULL;
		pFiles->m_szGID = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "list", &(pFiles->m_szFileList));
			attrSetStr(pAttr, "uid", &(pFiles->m_szUID));
			attrSetStr(pAttr, "gid", &(pFiles->m_szGID));
		} while ((pAttr = pAttr->m_pNext));
	}

	return pFiles;
}

int freeXMLFiles(t_structXMLFiles** ppFiles)
{
	if (*ppFiles) {
		freeStr(&((*ppFiles)->m_szFileList));
		freeStr(&((*ppFiles)->m_szUID));
		freeStr(&((*ppFiles)->m_szGID));
		free(*ppFiles);
	}
	*ppFiles = NULL;

	return 0;
}

t_structXMLPackage* newXMLPackage(const t_structXMLAttr* pAttrs)
{
	t_structXMLPackage* pPackage = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pPackage = malloc(sizeof(t_structXMLPackage)))) {
		pPackage->m_szName = NULL;
		pPackage->m_szGroup = NULL;
		pPackage->m_szSummary = NULL;
		pPackage->m_szDescription = NULL;
		pPackage->m_pPre = NULL;
		pPackage->m_pPost = NULL;
		pPackage->m_pPreUn = NULL;
		pPackage->m_pPostUn = NULL;
		pPackage->m_pVerify = NULL;
		pPackage->m_nAutoRequire = 1;
		pPackage->m_nAutoProvide = 1;
		pPackage->m_nAutoSuggest = 0;
		pPackage->m_pFiles = NULL;
		pPackage->m_pRequires = NULL;
		pPackage->m_pSuggests = NULL;
		pPackage->m_pObsoletes = NULL;
		pPackage->m_pProvides = NULL;
		pPackage->m_pNext = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "name", &(pPackage->m_szName));
			attrSetStr(pAttr, "group", &(pPackage->m_szGroup));
			attrSetBool(pAttr, "autorequire", &(pPackage->m_nAutoRequire));
			attrSetBool(pAttr, "autoprovide", &(pPackage->m_nAutoProvide));
			attrSetBool(pAttr, "autosuggest", &(pPackage->m_nAutoSuggest));
		} while ((pAttr = pAttr->m_pNext));
	}

	return pPackage;
}

int freeXMLPackage(t_structXMLPackage** ppPackage)
{
	if (*ppPackage) {
		freeStr(&((*ppPackage)->m_szName));
		freeStr(&((*ppPackage)->m_szGroup));
		freeStr(&((*ppPackage)->m_szSummary));
		freeStr(&((*ppPackage)->m_szDescription));
		freeXMLFiles(&((*ppPackage)->m_pFiles));
		freeXMLScripts(&((*ppPackage)->m_pPre));
		freeXMLScripts(&((*ppPackage)->m_pPost));
		freeXMLScripts(&((*ppPackage)->m_pPreUn));
		freeXMLScripts(&((*ppPackage)->m_pPostUn));
		freeXMLScripts(&((*ppPackage)->m_pVerify));
		freeXMLRequire(&((*ppPackage)->m_pRequires));
		freeXMLRequire(&((*ppPackage)->m_pSuggests));
		freeXMLRequire(&((*ppPackage)->m_pObsoletes));
		freeXMLRequire(&((*ppPackage)->m_pProvides));
		freeXMLPackage(&((*ppPackage)->m_pNext));
		free(*ppPackage);
	}
	*ppPackage = NULL;

	return 0;
}

t_structXMLPackage* addXMLPackage(const t_structXMLAttr* pAttrs,
				  t_structXMLPackage** ppPackage)
{
	t_structXMLPackage* pPackage = NULL;

	if ((pPackage = getLastXMLPackage(*ppPackage)))
		pPackage = (pPackage->m_pNext = newXMLPackage(pAttrs));
	else
		pPackage = (*ppPackage = newXMLPackage(pAttrs));

	return pPackage;
}

t_structXMLPackage* getLastXMLPackage(t_structXMLPackage* pPackage)
{
	while (pPackage && (pPackage->m_pNext))
		pPackage = pPackage->m_pNext;

	return pPackage;
}

t_structXMLChange* newXMLChange(const t_structXMLAttr* pAttrs)
{
	t_structXMLChange* pChange = NULL;

	if ((pChange = malloc(sizeof(t_structXMLChange)))) {
		pChange->m_szValue = NULL;
		pChange->m_pNext = NULL;
	}

	return pChange;
}

int freeXMLChange(t_structXMLChange** ppChange)
{
	if (*ppChange) {
		freeStr(&((*ppChange)->m_szValue));
		freeXMLChange(&((*ppChange)->m_pNext));
		free(*ppChange);
	}
	*ppChange = NULL;

	return 0;
}

t_structXMLChange* addXMLChange(const t_structXMLAttr* pAttrs,
				t_structXMLChange** ppChange)
{
	t_structXMLChange* pChange = NULL;

	if ((pChange = getLastXMLChange(*ppChange)))
		pChange = (pChange->m_pNext = newXMLChange(pAttrs));
	else
		pChange = (*ppChange = newXMLChange(pAttrs));
	return pChange;
}

t_structXMLChange* getLastXMLChange(t_structXMLChange* pChange)
{
	while (pChange && (pChange->m_pNext))
		pChange = pChange->m_pNext;

	return pChange;
}

t_structXMLChanges* newXMLChanges(const t_structXMLAttr* pAttrs)
{
	t_structXMLChanges* pChanges = NULL;
	t_structXMLAttr* pAttr = NULL;

	if ((pChanges = malloc(sizeof(t_structXMLChanges)))) {
		pChanges->m_szDate = NULL;
		pChanges->m_szVersion = NULL;
		pChanges->m_szAuthor = NULL;
		pChanges->m_szAuthorEmail = NULL;
		pChanges->m_pChanges = NULL;
		pChanges->m_pNext = NULL;

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "date", &(pChanges->m_szDate));
			attrSetStr(pAttr, "version", &(pChanges->m_szVersion));
			attrSetStr(pAttr, "author", &(pChanges->m_szAuthor));
			attrSetStr(pAttr, "author-email", &(pChanges->m_szAuthorEmail));
		} while ((pAttr = pAttr->m_pNext));
	}

	return pChanges;
}

int freeXMLChanges(t_structXMLChanges** ppChanges)
{
	if (*ppChanges) {
		freeStr(&((*ppChanges)->m_szDate));
		freeStr(&((*ppChanges)->m_szVersion));
		freeStr(&((*ppChanges)->m_szAuthor));
		freeStr(&((*ppChanges)->m_szAuthorEmail));
		freeXMLChanges(&((*ppChanges)->m_pNext));
		free(*ppChanges);
	}
	*ppChanges = NULL;

	return 0;
}

t_structXMLChanges* addXMLChanges(const t_structXMLAttr* pAttrs,
				  t_structXMLChanges** ppChanges)
{
	t_structXMLChanges* pChanges = NULL;

	if ((pChanges = getLastXMLChanges(*ppChanges)))
		pChanges = (pChanges->m_pNext = newXMLChanges(pAttrs));
	else
		pChanges = (*ppChanges = newXMLChanges(pAttrs));

	return pChanges;
}

t_structXMLChanges* getLastXMLChanges(t_structXMLChanges* pChanges)
{
	while (pChanges && (pChanges->m_pNext))
		pChanges = pChanges->m_pNext;

	return pChanges;
}

t_structXMLSpec* newXMLSpec(const t_structXMLAttr* pAttrs,
			    const char* szXMLFile)
{
	t_structXMLSpec* pSpec = NULL;
	t_structXMLAttr* pAttr = NULL;
	char* szTmp = NULL;
	int nLen;

	if ((pSpec = malloc(sizeof(t_structXMLSpec)))) {
		pSpec->m_szSpecName = NULL;
		pSpec->m_szBuildRootDir = NULL;
		pSpec->m_szBuildSubdir = NULL;
		pSpec->m_szRootDir = NULL;
		pSpec->m_szName = NULL;
		pSpec->m_szVersion = NULL;
		pSpec->m_szRelease = NULL;
		pSpec->m_szEpoch = NULL;
		pSpec->m_szDistribution = NULL;
		pSpec->m_szVendor = NULL;
		pSpec->m_szPackager = NULL;
		pSpec->m_szPackagerEmail = NULL;
		pSpec->m_szCopyright = NULL;
		pSpec->m_szURL = NULL;
		pSpec->m_pBuildRequires = NULL;
		pSpec->m_pBuildConflicts = NULL;
		pSpec->m_pBuildSuggests = NULL;
		pSpec->m_pPrep = NULL;
		pSpec->m_pBuild = NULL;
		pSpec->m_pInstall = NULL;
		pSpec->m_pClean = NULL;
		pSpec->m_pMacros = NULL;
		pSpec->m_pSources = NULL;
		pSpec->m_pPatches = NULL;
		pSpec->m_pPackages = NULL;
		pSpec->m_pChangelog = NULL;
		g_pSpec = newSpec();

		pAttr = (t_structXMLAttr*)pAttrs;
		do {
			attrSetStr(pAttr, "name", &(pSpec->m_szName));
			attrSetStr(pAttr, "version", &(pSpec->m_szVersion));
			attrSetStr(pAttr, "release", &(pSpec->m_szRelease));
			attrSetStr(pAttr, "epoch", &(pSpec->m_szEpoch));
			attrSetStr(pAttr, "distribution", &(pSpec->m_szDistribution));
			attrSetStr(pAttr, "vendor", &(pSpec->m_szVendor));
			attrSetStr(pAttr, "packager", &(pSpec->m_szPackager));
			attrSetStr(pAttr, "packager-email", &(pSpec->m_szPackagerEmail));
			attrSetStr(pAttr, "copyright", &(pSpec->m_szCopyright));
			attrSetStr(pAttr, "url", &(pSpec->m_szURL));
			attrSetStr(pAttr, "buildroot", &(pSpec->m_szBuildRootDir));
			attrSetStr(pAttr, "builddir", &(pSpec->m_szBuildSubdir));
		} while ((pAttr = pAttr->m_pNext));

		addMacro(NULL, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
		szTmp = (char*)rpmGetPath(szXMLFile, NULL);
		newStr(szTmp, &(pSpec->m_szSpecName));
		free(szTmp);
		if ((szTmp = rindex(pSpec->m_szSpecName, '/'))) {
			szTmp++;
			nLen = szTmp-(pSpec->m_szSpecName);
			g_szSpecPath = malloc(nLen+1);
			snprintf(g_szSpecPath, nLen, "%s", pSpec->m_szSpecName);
		}
		szTmp = malloc(4097);
		if (pSpec->m_szBuildRootDir == NULL) {
			snprintf(szTmp, 4096, "%%{_tmppath}/rpmxml-%s-%s", pSpec->m_szName,
									   pSpec->m_szVersion);
			newStrEx(szTmp, &(pSpec->m_szBuildRootDir));
		}
		if (pSpec->m_szBuildSubdir == NULL) {
			snprintf(szTmp, 4096, "%s-%s", pSpec->m_szName,
						       pSpec->m_szVersion);
			newStrEx(szTmp, &(pSpec->m_szBuildSubdir));
		}
		free(szTmp);
		if (pSpec->m_szRootDir == NULL)
			newStrEx("/", &(pSpec->m_szRootDir));
		addMacroEx("name", (char**)&(pSpec->m_szName), RMIL_SPEC);
		addMacroEx("version", (char**)&(pSpec->m_szVersion), RMIL_SPEC);
		addMacroEx("PACKAGE_VERSION", (char**)&(pSpec->m_szVersion), -1);
		addMacroEx("release", (char**)&(pSpec->m_szRelease), RMIL_SPEC);
		addMacroEx("PACKAGE_RELEASE", (char**)&(pSpec->m_szRelease), -2);
		addMacroEx("copyright", (char**)&(pSpec->m_szCopyright), RMIL_SPEC);
		addMacroEx("url", (char**)&(pSpec->m_szURL), RMIL_SPEC);
		addMacroEx("distribution", (char**)&(pSpec->m_szDistribution), RMIL_SPEC);
		addMacroEx("vendor", (char**)&(pSpec->m_szVendor), RMIL_SPEC);
		addMacroEx("buildroot", (char**)&(pSpec->m_szBuildRootDir), RMIL_SPEC);
		addMacroEx("buildsubdir", (char**)&(pSpec->m_szBuildSubdir), RMIL_SPEC);
		if (pSpec->m_szPackagerEmail || pSpec->m_szPackager) {
			szTmp = malloc(strlen(pSpec->m_szPackager)+strlen(pSpec->m_szPackagerEmail)+4);
			if (pSpec->m_szPackagerEmail && pSpec->m_szPackager)
				sprintf(szTmp, "%s <%s>", pSpec->m_szPackager, pSpec->m_szPackagerEmail);
			else
				sprintf(szTmp, "%s", pSpec->m_szPackager ? pSpec->m_szPackager : pSpec->m_szPackagerEmail);
			addMacroEx("packager", (char**)&(szTmp), RMIL_SPEC);
			free(szTmp);
		}
	}

	return pSpec;
}

int freeXMLSpec(t_structXMLSpec** ppSpec)
{
	if (*ppSpec) {
		freeStr(&((*ppSpec)->m_szSpecName));
		freeStr(&((*ppSpec)->m_szBuildRootDir));
		freeStr(&((*ppSpec)->m_szBuildSubdir));
		freeStr(&((*ppSpec)->m_szRootDir));
		freeStr(&((*ppSpec)->m_szName));
		freeStr(&((*ppSpec)->m_szVersion));
		freeStr(&((*ppSpec)->m_szRelease));
		freeStr(&((*ppSpec)->m_szEpoch));
		freeStr(&((*ppSpec)->m_szDistribution));
		freeStr(&((*ppSpec)->m_szVendor));
		freeStr(&((*ppSpec)->m_szPackager));
		freeStr(&((*ppSpec)->m_szPackagerEmail));
		freeStr(&((*ppSpec)->m_szCopyright));
		freeStr(&((*ppSpec)->m_szURL));
		freeXMLRequire(&((*ppSpec)->m_pBuildRequires));
		freeXMLRequire(&((*ppSpec)->m_pBuildConflicts));
		freeXMLRequire(&((*ppSpec)->m_pBuildSuggests));
		freeXMLScripts(&((*ppSpec)->m_pPrep));
		freeXMLScripts(&((*ppSpec)->m_pBuild));
		freeXMLScripts(&((*ppSpec)->m_pInstall));
		freeXMLScripts(&((*ppSpec)->m_pClean));
		freeXMLMacro(&((*ppSpec)->m_pMacros));
		freeXMLSource(&((*ppSpec)->m_pSources));
		freeXMLSource(&((*ppSpec)->m_pPatches));
		freeXMLPackage(&((*ppSpec)->m_pPackages));
		freeXMLChanges(&((*ppSpec)->m_pChangelog));
		if (g_pSpec)
			freeSpec(g_pSpec);
		g_pSpec = NULL;
		if (g_szSpecPath)
			freeStr(&g_szSpecPath);
		free(*ppSpec);
	}
	*ppSpec = NULL;

	return 0;
}
