#ifndef _XML_STRUCT_H_
#define _XML_STRUCT_H_

#include "xmlmisc.h"

typedef struct structXMLAttr
{
	char*                 m_szName;
	char*                 m_szValue;
	int                   m_nType;
	struct structXMLAttr* m_pNext;
} t_structXMLAttr;

typedef struct structXMLMacro
{
	char*                  m_szName;
	char*                  m_szValue;
	struct structXMLMacro* m_pNext;
} t_structXMLMacro;

typedef struct structXMLMirror
{
	char*                   m_szName;
	char*                   m_szLocation;
	char*                   m_szCountry;
	char*                   m_szPath;
	int                     m_nTries;
	struct structXMLMirror* m_pNext;
} t_structXMLMirror;

typedef struct structXMLSource
{
	char*                   m_szName;
	int                     m_nSize;
	char*                   m_szMD5;
	char*                   m_szDirectory;
	int                     m_nNum;
	struct structXMLMirror* m_pMirrors;
	struct structXMLSource* m_pNext;
} t_structXMLSource;

typedef struct structXMLRequire
{
	char*                    m_szName;
	char*                    m_szVersion;
	char*                    m_szRelease;
	char*                    m_szEpoch;
	char*                    m_szCompare;
	struct structXMLRequire* m_pNext;
} t_structXMLRequire;

typedef struct structXMLScript
{
	char*                   m_szInterpreter;
	char*                   m_szScript;
	char*                   m_szEntry;
	struct structXMLScript* m_pNext;
} t_structXMLScript;

typedef struct structXMLScripts
{
	char*                   m_szInterpreter;
	char*                   m_szScript;
	struct structXMLScript* m_pScripts;
} t_structXMLScripts;

typedef struct structXMLFiles
{
	char* m_szFileList;
	char* m_szUID;
	char* m_szGID;
} t_structXMLFiles;

typedef struct structXMLPackage
{
	char*                    m_szName;
	char*                    m_szGroup;
	char*                    m_szSummary;
	char*                    m_szDescription;
	int                      m_nAutoRequire;
	int                      m_nAutoProvide;
	int                      m_nAutoSuggest;
	struct structXMLFiles*   m_pFiles;
	struct structXMLScripts* m_pPre;
	struct structXMLScripts* m_pPost;
	struct structXMLScripts* m_pPreUn;
	struct structXMLScripts* m_pPostUn;
	struct structXMLScripts* m_pVerify;
	struct structXMLRequire* m_pRequires;
	struct structXMLRequire* m_pSuggests;
	struct structXMLRequire* m_pObsoletes;
	struct structXMLRequire* m_pProvides;
	struct structXMLPackage* m_pNext;
} t_structXMLPackage;

typedef struct structXMLChange
{
	char*                   m_szValue;
	struct structXMLChange* m_pNext;
} t_structXMLChange;

typedef struct structXMLChanges
{
	char*                    m_szDate;
	char*                    m_szVersion;
	char*                    m_szAuthor;
	char*                    m_szAuthorEmail;
	struct structXMLChange*  m_pChanges;
	struct structXMLChanges* m_pNext;
} t_structXMLChanges;

typedef struct structXMLSpec
{
	char*                    m_szSpecName;
	char*                    m_szBuildRootDir;
	char*                    m_szBuildSubdir;
	char*                    m_szRootDir;
	char*                    m_szName;
	char*                    m_szVersion;
	char*                    m_szRelease;
	char*                    m_szEpoch;
	char*                    m_szDistribution;
	char*                    m_szVendor;
	char*                    m_szPackager;
	char*                    m_szPackagerEmail;
	char*                    m_szCopyright;
	char*                    m_szURL;
	struct structXMLRequire* m_pBuildRequires;
	struct structXMLRequire* m_pBuildConflicts;
	struct structXMLRequire* m_pBuildSuggests;
	struct structXMLScripts* m_pPrep;
	struct structXMLScripts* m_pBuild;
	struct structXMLScripts* m_pInstall;
	struct structXMLScripts* m_pClean;
	struct structXMLMacro*   m_pMacros;
	struct structXMLSource*  m_pSources;
	struct structXMLSource*  m_pPatches;
	struct structXMLPackage* m_pPackages;
	struct structXMLChanges* m_pChangelog;
} t_structXMLSpec;

void attrSetStr(const t_structXMLAttr* pAttr,
		const char* szParam,
		char** pszVar);
void attrSetInt(const t_structXMLAttr* pAttr,
		const char* szParam,
		int* pnVar);
void attrSetBool(const t_structXMLAttr* pAttr,
		 const char* szParam,
		 int* pnVar);

t_structXMLAttr* newXMLAttr(const char* szName,
			    const char* szValue,
			    int nType);
int freeXMLAttr(t_structXMLAttr** ppAttr);
t_structXMLAttr* addXMLAttr(const char* szName,
			    const char* szValue,
			    int nType,
			    t_structXMLAttr** ppAttr);
t_structXMLAttr* getLastXMLAttr(t_structXMLAttr* pAttr);
t_structXMLAttr* getXMLAttr(const char* szName,
			    t_structXMLAttr* pAttr);

t_structXMLSpec* parseXMLSpec(const char* szFile, int nVerbose);

t_structXMLMacro* newXMLMacro(const t_structXMLAttr* pAttrs);
int freeXMLMacro(t_structXMLMacro** ppMacro);
t_structXMLMacro* addXMLMacro(const t_structXMLAttr* pAttrs,
			      t_structXMLMacro** ppMacro);
t_structXMLMacro* getLastXMLMacro(t_structXMLMacro* pMacro);

t_structXMLMirror* newXMLMirror(const t_structXMLAttr* pAttrs);
int freeXMLMirror(t_structXMLMirror** ppMirror);
t_structXMLMirror* addXMLMirror(const t_structXMLAttr* pAttrs,
				t_structXMLMirror** ppMirror);
t_structXMLMirror* getLastXMLMirror(t_structXMLMirror* pMirror);

t_structXMLSource* newXMLSource(const t_structXMLAttr* pAttrs);
int freeXMLSource(t_structXMLSource** ppSource);
t_structXMLSource* addXMLSource(const t_structXMLAttr* pAttrs,
				t_structXMLSource** ppSource);
t_structXMLSource* getLastXMLSource(t_structXMLSource* pSource);
t_structXMLSource* getXMLSource(int nNum,
				t_structXMLSource* pSource);

t_structXMLRequire* newXMLRequire(const t_structXMLAttr* pAttrs);
int freeXMLRequire(t_structXMLRequire** ppRequire);
t_structXMLRequire* addXMLRequire(const t_structXMLAttr* pAttrs,
				  t_structXMLRequire** ppRequire);
t_structXMLRequire* getLastXMLRequire(t_structXMLRequire* pRequire);

t_structXMLScript* newXMLScript(const t_structXMLAttr* pAttrs);
int freeXMLScript(t_structXMLScript** ppScript);
t_structXMLScript* addXMLScript(const t_structXMLAttr* pAttrs,
				t_structXMLScript** ppScript);
t_structXMLScript* getLastXMLScript(t_structXMLScript* pScript);

t_structXMLScripts* newXMLScripts(const t_structXMLAttr* pAttrs);
int freeXMLScripts(t_structXMLScripts** ppScripts);

t_structXMLFiles* newXMLFiles(const t_structXMLAttr* pAttrs);
int freeXMLFiles(t_structXMLFiles** ppFiles);

t_structXMLPackage* newXMLPackage(const t_structXMLAttr* pAttrs);
int freeXMLPackage(t_structXMLPackage** ppPackage);
t_structXMLPackage* addXMLPackage(const t_structXMLAttr* pAttrs,
				  t_structXMLPackage** ppPackage);
t_structXMLPackage* getLastXMLPackage(t_structXMLPackage* pPackage);

t_structXMLChange* newXMLChange(const t_structXMLAttr* pAttrs);
int freeXMLChange(t_structXMLChange** ppChange);
t_structXMLChange* addXMLChange(const t_structXMLAttr* pAttrs,
				t_structXMLChange** ppChange);
t_structXMLChange* getLastXMLChange(t_structXMLChange* pChange);

t_structXMLChanges* newXMLChanges(const t_structXMLAttr* pAttrs);
int freeXMLChanges(t_structXMLChanges** ppChanges);
t_structXMLChanges* addXMLChanges(const t_structXMLAttr* pAttrs,
				  t_structXMLChanges** ppChanges);
t_structXMLChanges* getLastXMLChanges(t_structXMLChanges* pChanges);

t_structXMLSpec* newXMLSpec(const t_structXMLAttr* pAttrs,
			    const char* szXMLFile);
int freeXMLSpec(t_structXMLSpec** ppSpec);

#endif
