// standard library includes
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// standard C++ includes
#include <fstream>
#include <string>

// 3rd party library includes
#include <expat.h>

// our includes
#include "XMLSpec.h"

// this is the maximum tree dept we will require as
// well as the number of scructures a specific tag can
// occur in
#define XML_MAX_TREE_DEPTH 5
#define XML_MAX_PRE_TAGS   10

using namespace std;

// these define the error values
enum eXMLErr
{
	XMLERR_WARNING = 0x0000,
	XMLERR_ERROR,
	XMLERR_FATAL
};

// this defines the structure that we are using in the callback
// containing all the parse-specific data we might need.
struct structCBData
{
	XML_Parser*  m_pParser;
	string       m_sFilename;
	int          m_nWarnings;
	int          m_nErrors;
	int          m_nDepth;
	unsigned int m_pnTree[XML_MAX_TREE_DEPTH];
	string       m_sData;
	XMLSpec*     m_pSpec;
	XMLAttrs*    m_pAttrs;
};

// enumeration of all XML tags that are recognised
enum enumXMLTAGValid
{
	XTAG_NO          = 0x0000,
	XTAG_WRONGSTRUCT,
	XTAG_SPEC,
	XTAG_EXCLUDEARCH,
	XTAG_EXCLUDEOS,
	XTAG_EXCLUSIVEARCH,
	XTAG_EXCLUSIVEOS,
	XTAG_SOURCE,
	XTAG_PATCH,
	XTAG_NOSOURCE,
	XTAG_MIRROR,
	XTAG_PACKAGE,
	XTAG_SUMMARY,
	XTAG_DESC,
	XTAG_REQS,
	XTAG_BREQS,
	XTAG_PROVS,
	XTAG_OBSOLS,
	XTAG_FILES,
	XTAG_FILE,
	XTAG_MACRO,
	XTAG_PREP,
	XTAG_SETUP,
	XTAG_BUILD,
	XTAG_INSTALL,
	XTAG_CLEAN,
	XTAG_PRE,
	XTAG_POST,
	XTAG_PREUN,
	XTAG_POSTUN,
	XTAG_VERIFY,
	XTAG_SCRIPT,
	XTAG_CHANGELOG,
	XTAG_CHANGES,
	XTAG_CHANGE,
	XTAG_ANY         = 0xFFFF // this always needs to be the last entry
};

// this is a structure to match tags to their values
struct structXMLMatch
{
	unsigned int m_nVal;
	int          m_pnDepth[XML_MAX_TREE_DEPTH+1];
	unsigned int m_pnFollows[XML_MAX_PRE_TAGS+1];
	char*        m_szName;
};

// this allows for the matching of all tags
structXMLMatch g_pMatches[] =
{
	{
		XTAG_SPEC,         { 0, -1, -1},    {
												XTAG_ANY,
												XTAG_NO
											},
		"spec"
	},
	{
		XTAG_PACKAGE,      { 1,  3, -1},    {
												XTAG_SPEC,
												XTAG_REQS,
												XTAG_BREQS,
												XTAG_PROVS,
												XTAG_OBSOLS,
												XTAG_NO
											},
		"package"
	},
	{
		XTAG_SOURCE,       { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"source"
	},
	{
		XTAG_PATCH,        { 1,  2, -1},    {
												XTAG_SPEC,
												XTAG_PREP,
												XTAG_NO
											},
		"patch"
	},
	{
		XTAG_NOSOURCE,     { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"nosource"
	},
	{
		XTAG_MIRROR,       { 2, -1, -1},    {
												XTAG_SOURCE,
												XTAG_NO
											},
		"mirror"
	},
	{
		XTAG_SUMMARY,      { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"summary"
	},
	{
		XTAG_DESC,         { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"description"
	},
	{
		XTAG_REQS,         { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"requires"
	},
	{
		XTAG_BREQS,        { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"buildrequires"
	},
	{
		XTAG_PROVS,        { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"provides"
	},
	{
		XTAG_OBSOLS,       { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"obsoletes"
	},
	{
		XTAG_FILES,        { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"files"
	},
	{
		XTAG_FILE,         { 3, -1, -1},    {
												XTAG_FILES,
												XTAG_NO
											},
		"file"
	},
	{
		XTAG_MACRO,        { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"macro"
	},
	{
		XTAG_PREP,         { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"prep"
	},
	{
		XTAG_SETUP,        { 2, -1, -1},    {
												XTAG_PREP,
												XTAG_NO
											},
		"setup"
	},
	{
		XTAG_BUILD,        { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"build"
	},
	{
		XTAG_INSTALL,      { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"install"
	},
	{
		XTAG_CLEAN,        { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"clean"
	},
	{
		XTAG_PRE,          { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"pre"
	},
	{
		XTAG_POST,         { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"post"
	},
	{
		XTAG_PREUN,       { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"preun"
	},
	{
		XTAG_POSTUN,       { 2, -1, -1},    {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"postun"
	},
	{
		XTAG_VERIFY,        { 2, -1, -1},   {
												XTAG_PACKAGE,
												XTAG_NO
											},
		"verify"
	},
	{
		XTAG_SCRIPT,       { 2,  3, -1},    {
												XTAG_PREP,
												XTAG_BUILD,
												XTAG_INSTALL,
												XTAG_CLEAN,
												XTAG_PRE,
												XTAG_POST,
												XTAG_PREUN,
												XTAG_POSTUN,
												XTAG_VERIFY,
												XTAG_NO
											},
		"script"
	},
	{
		XTAG_CHANGELOG,    { 1, -1, -1},    {
												XTAG_SPEC,
												XTAG_NO
											},
		"changelog"
	},
	{
		XTAG_CHANGES,      { 2, -1, -1},    {
												XTAG_CHANGELOG,
												XTAG_NO
											},
		"changes"
	},
	{
		XTAG_CHANGE,       { 3, -1, -1},    {
												XTAG_CHANGES,
												XTAG_NO
											},
		"change"
	},
	// this always needs to be the last entry
	{
		XTAG_NO,           {-1, -1, -1},    {
												XTAG_NO
											},
		"none"
	}
};

const char* treeToString(unsigned int* pnTree,
						 int nDepth)
{
	// internal string storage
	string sTree;

	// build the tree to the specified depth
	for (int i = 0; i < nDepth; i++) {
		int j = 0;
		while (g_pMatches[j].m_nVal != pnTree[i])
			j++;
		sTree += string("<") + string(g_pMatches[j].m_szName) + string(">");
	}

	// return the tree string
	return sTree.c_str();
}

unsigned int getTagValue(const char* szElement,
						 int nDepth,
						 unsigned int nPrev)
{
	// first convert the tag to a tag value
	unsigned int nTagVal = XTAG_NO;

	// loop through all matches to see if we have a valid
	// tag here
	int i = 0;
	do {
		// test if we have a match
		if (strcasecmp(g_pMatches[i].m_szName, szElement) == 0) {
			// look for a match on the tree depth
			int j = 0;
			nTagVal = XTAG_WRONGSTRUCT;
			while (g_pMatches[i].m_pnDepth[j] != -1) {
				if (g_pMatches[i].m_pnDepth[j++] == nDepth) {
					j = 0;
					do {
						if (g_pMatches[i].m_pnFollows[j] == nPrev) {
							nTagVal = g_pMatches[i].m_nVal;
							break;
						}
					} while (g_pMatches[i].m_pnFollows[++j] != XTAG_NO);
					break;
				}
			}
			// break out
			break;
		}
	} while (g_pMatches[++i].m_nVal != XTAG_NO);

	// return
	return nTagVal;
}

void createError(int nErrType,
				 structCBData* pData,
				 const char* szFormat, ...)
{
	// one more error/warning
	nErrType == XMLERR_WARNING ? pData->m_nWarnings++ : pData->m_nErrors++;

	// setup internal variables
	FILE* fOut = stderr;
	switch (nErrType) {
		case XMLERR_WARNING:
			fOut = stdout;
			fprintf(fOut, "%s(%d): warning: ", pData->m_sFilename.c_str(),
						  XML_GetCurrentLineNumber(*(pData->m_pParser)));
			break;
		case XMLERR_ERROR:
			fprintf(fOut, "%s(%d): error:   ", pData->m_sFilename.c_str(),
						  XML_GetCurrentLineNumber(*(pData->m_pParser)));
			break;
		case XMLERR_FATAL:
			fprintf(fOut, "%s(%d): fatal:   ", pData->m_sFilename.c_str(),
						  XML_GetCurrentLineNumber(*(pData->m_pParser)));
			break;
		default:
			return;
	}

	// create the argument list and print
	va_list vaArgList;
	va_start(vaArgList, szFormat);
	vfprintf(fOut, szFormat, vaArgList);
	fprintf(fOut, "\n");
}

void startDepth0(structCBData* pData)
{
	// this indicates a spec start
	if (pData->m_pnTree[0] == XTAG_SPEC) {
		// if we have a spec already, we are in trouble
		if (pData->m_pSpec)
			createError(XMLERR_ERROR, pData, "Extra 'spec' tag found.");
		else if (!(pData->m_pSpec = XMLSpec::parseCreate(pData->m_pAttrs,
														 pData->m_sFilename.c_str())))
			createError(XMLERR_ERROR, pData,
						"Failed to parse 'spec' tag (%s).",
						pData->m_pAttrs->getError());
		else if (pData->m_pAttrs->hasWarning())
			createError(XMLERR_WARNING, pData, pData->m_pAttrs->getWarning());
	}
}

void startDepth1(structCBData* pData)
{
	// make sure we have a spec
	if (!pData->m_pSpec) {
		createError(XMLERR_ERROR, pData, "Spec has not been defined.");
		return;
	}

	// hanlde tag
	switch (pData->m_pnTree[1]) {
		case XTAG_PACKAGE:
			if (!XMLPackage::parseCreate(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'package' tag or package already exists (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_SOURCE:
			if (!XMLSource::parseCreate(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'source' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_PATCH:
			if (!XMLPatch::parseCreate(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'patch' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_NOSOURCE:
			if (!XMLNoSource::parseCreate(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'nosource' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_CHANGELOG:
		case XTAG_PREP:
		case XTAG_BUILD:
		case XTAG_INSTALL:
		case XTAG_CLEAN:
		default:
			break;
	}
}

void startDepth2(structCBData* pData)
{
	// make sure we have a spec
	if (!pData->m_pSpec) {
		createError(XMLERR_ERROR, pData, "Spec has not been defined.");
		return;
	}

	// handle tag
	switch (pData->m_pnTree[2]) {
		case XTAG_MIRROR:
			switch (pData->m_pnTree[1]) {
				case XTAG_SOURCE:
					if (!XMLMirror::parseCreate(pData->m_pAttrs, pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'mirror' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_PATCH:
					if (!XMLMirror::parseCreate(pData->m_pAttrs, pData->m_pSpec, true))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'mirror' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
			}
			break;
		case XTAG_FILES:
			if (!XMLFiles::parseCreate(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'change' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_CHANGES:
			if (!XMLChangelogDate::parseCreate(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'change' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_PRE:
			if (!XMLPackageScripts::createPreScripts(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'pre' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_POST:
			if (!XMLPackageScripts::createPostScripts(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'pre' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_PREUN:
			if (!XMLPackageScripts::createPreUnScripts(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'pre' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_POSTUN:
			if (!XMLPackageScripts::createPostUnScripts(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'pre' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_VERIFY:
			if (!XMLPackageScripts::createVerifyScripts(pData->m_pAttrs, pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to parse 'pre' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_REQS:
		case XTAG_BREQS:
		case XTAG_PROVS:
		case XTAG_OBSOLS:
		default:
			break;
	}
}

void startDepth3(structCBData* pData)
{
	// make sure we have a spec
	if (!pData->m_pSpec) {
		createError(XMLERR_ERROR, pData, "Spec has not been defined.");
		return;
	}

	// handle tag
	switch (pData->m_pnTree[3]) {
		case XTAG_PACKAGE:
			switch (pData->m_pnTree[2]) {
				case XTAG_REQS:
					if (!XMLPackageContainer::addRequire(pData->m_pAttrs,
														 pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
							"Failed to parse 'package' tag (%s).",
							pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_BREQS:
					if (!XMLPackageContainer::addBuildRequire(pData->m_pAttrs,
															  pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
							"Failed to parse 'package' tag (%s).",
							pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_PROVS:
					if (!XMLPackageContainer::addProvide(pData->m_pAttrs,
														 pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
							"Failed to parse 'package' tag (%s).",
							pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_OBSOLS:
					if (!XMLPackageContainer::addObsolete(pData->m_pAttrs,
														  pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
							"Failed to parse 'package' tag (%s).",
							pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

void startElementCB(void* pCBData,
					const char* szElement,
					const char** szAttr)
{
	// get the data structure we are working with and
	structCBData* pData = (structCBData*)pCBData;

	// validate and get the tag we are working with
	unsigned int nTag = getTagValue(szElement,
									pData->m_nDepth,
									pData->m_nDepth ? pData->m_pnTree[pData->m_nDepth-1] : XTAG_ANY);

	if (nTag == XTAG_NO || nTag == XTAG_WRONGSTRUCT)
		return createError(XMLERR_WARNING, pData, "Unexpected tag '%s' in structure '%s'.",
						  szElement,
						  treeToString(pData->m_pnTree, pData->m_nDepth++));
	pData->m_pnTree[pData->m_nDepth] = nTag;
	pData->m_sData.assign("");

	if (pData->m_pAttrs)
		delete pData->m_pAttrs;
	pData->m_pAttrs = new XMLAttrs(szAttr);

	switch (pData->m_nDepth++) {
		case 0:
			startDepth0(pData);
			break;
		case 1:
			startDepth1(pData);
			break;
		case 2:
			startDepth2(pData);
			break;
		case 3:
			startDepth3(pData);
			break;
		default:
			break;
	}
}

void endDepth1(structCBData* pData)
{
	// make sure we have a spec
	if (!pData->m_pSpec) {
		createError(XMLERR_ERROR, pData, "Spec has not been defined.");
		return;
	}

	// handle tag
	switch (pData->m_pnTree[1]) {
		case XTAG_MACRO:
			if (!XMLMacro::parseCreate(pData->m_pAttrs,
									   pData->m_sData.c_str(),
									   pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to add macro entry (%s).",
							pData->m_pAttrs->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		default:
			break;
	}
}

void endDepth2(structCBData* pData)
{
	// make sure we have a spec
	if (!pData->m_pSpec) {
		createError(XMLERR_ERROR, pData, "Spec has not been defined.");
		return;
	}

	// do the tag
	switch (pData->m_pnTree[2]) {
		case XTAG_SUMMARY:
			if (!XMLPackage::addSummary(pData->m_pAttrs, pData->m_sData.c_str(),
										pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to add 'summary'.");
			break;
		case XTAG_DESC:
			if (!XMLPackage::addDescription(pData->m_pAttrs, pData->m_sData.c_str(),
											pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to add 'description'.");
			break;
		case XTAG_SCRIPT:
			switch (pData->m_pnTree[1]) {
				case XTAG_PREP:
					if (!XMLScripts::addPrepScript(pData->m_pAttrs,
												   pData->m_sData.c_str(),
												   pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to add shell entry.");
					break;
				case XTAG_BUILD:
					if (!XMLScripts::addBuildScript(pData->m_pAttrs,
													pData->m_sData.c_str(),
													pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to add shell entry.");
					break;
				case XTAG_INSTALL:
					if (!XMLScripts::addInstallScript(pData->m_pAttrs,
													  pData->m_sData.c_str(),
													  pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to add shell entry.");
					break;
				case XTAG_CLEAN:
					if (!XMLScripts::addCleanScript(pData->m_pAttrs,
													pData->m_sData.c_str(),
													pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to add shell entry.");
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

void endDepth3(structCBData* pData)
{
	// make sure we have a spec
	if (!pData->m_pSpec) {
		createError(XMLERR_ERROR, pData, "Spec has not been defined.");
		return;
	}

	// handle tag
	switch (pData->m_pnTree[3]) {
		case XTAG_CHANGE:
			if (!XMLChangelogEntry::parseCreate(pData->m_pAttrs,
												pData->m_sData.c_str(),
												pData->m_pSpec))
				createError(XMLERR_ERROR, pData,
							"Failed to add changelog entry.");
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_FILE:
			if (!XMLFile::parseCreate(pData->m_pAttrs,
									  pData->m_sData.c_str(),
									  pData->m_pSpec))
				createError(XMLERR_ERROR, pData, "Failed to parse 'file' tag (%s).",
							pData->m_pSpec->getError());
			else if (pData->m_pSpec->hasWarning())
				createError(XMLERR_WARNING, pData, pData->m_pSpec->getWarning());
			break;
		case XTAG_SCRIPT:
			switch (pData->m_pnTree[2]) {
				case XTAG_PRE:
					if (!XMLPackageScripts::addPreScript(pData->m_pAttrs,
														 pData->m_sData.c_str(),
														 pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'script' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_POST:
					if (!XMLPackageScripts::addPostScript(pData->m_pAttrs,
														  pData->m_sData.c_str(),
														  pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'script' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_PREUN:
					if (!XMLPackageScripts::addPreUnScript(pData->m_pAttrs,
														   pData->m_sData.c_str(),
														   pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'script' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_POSTUN:
					if (!XMLPackageScripts::addPostUnScript(pData->m_pAttrs,
															pData->m_sData.c_str(),
															pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'script' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				case XTAG_VERIFY:
					if (!XMLPackageScripts::addVerifyScript(pData->m_pAttrs,
															pData->m_sData.c_str(),
															pData->m_pSpec))
						createError(XMLERR_ERROR, pData,
									"Failed to parse 'script' tag (%s).",
									pData->m_pSpec->getError());
					else if (pData->m_pSpec->hasWarning())
						createError(XMLERR_WARNING, pData,
									pData->m_pSpec->getWarning());
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

void endElementCB(void* pCBData,
				 const char* szElement)
{
	// get the data structure we are working with
	structCBData* pData = (structCBData*)pCBData;
	pData->m_nDepth--;

	// validate and get the tag we are working with
	unsigned int nTag = getTagValue(szElement,
									pData->m_nDepth,
									pData->m_nDepth ? pData->m_pnTree[pData->m_nDepth-1] : XTAG_ANY);
	if (nTag == XTAG_NO || nTag == XTAG_WRONGSTRUCT)
		return;

	// handle the tree depth
	switch (pData->m_nDepth) {
		case 0:
			break;
		case 1:
			endDepth1(pData);
			break;
		case 2:
			endDepth2(pData);
			break;
		case 3:
			endDepth3(pData);
			break;
		default:
			break;
	}

	// clean up
	pData->m_sData.assign("");
}

void characterCB(void* pCBData,
				 const char* szStr,
				 int nLen)
{
	// get the data structure we are working with
	structCBData* pData = (structCBData*)pCBData;

	// append the string to our internal data
	if (nLen) {
		char* szTmp = new char[nLen+1];
		strncpy(szTmp, szStr, nLen);
		szTmp[nLen] = '\0';
		pData->m_sData.append(szTmp);
		delete[] szTmp;
	}
}

int parseXMLSpec(const char* szXMLFilename,
				 XMLSpec*& pSpec)
{
	// create and setup our parser for use
	printf("Creating XML parser instance ... ");
	XML_Parser parser = XML_ParserCreate(NULL);
	if (!parser) {
		printf("Failed.\n\tERROR: Couldn't allocate memory for parser\n\n");
		return -1;
	}
	else
		printf("Ok.\n");
	XML_SetElementHandler(parser, startElementCB, endElementCB);
	XML_SetCharacterDataHandler(parser, characterCB);
	structCBData stData;
	stData.m_pParser = &parser;
	stData.m_sFilename.assign(szXMLFilename);
	stData.m_nWarnings = 0;
	stData.m_nErrors = 0;
	stData.m_nDepth = 0;
	stData.m_pSpec = NULL;
	stData.m_pAttrs = NULL;
	XML_SetUserData(parser, (void*)&stData);

	// open the input and output files here
	printf("Opening input XML spec ... ");
	ifstream fIn(szXMLFilename);
	if (!fIn.is_open()) {
		printf("Failed.\n\tERROR: Could not open %s\n\n", szXMLFilename);
		return -2;
	}
	else
		printf("Ok.\n");

	// parse our configuration (loop through file,
	// doing a break if needed (fatal error))
	printf("Parsing %s: \n", szXMLFilename);
	char szBuff[1024+1];
	unsigned int nLength = 0;
	while (!fIn.eof()) {
		fIn.get(szBuff, 1024, '\0');
		unsigned int nRead = strlen(szBuff);
		nLength += nRead;
		if (!XML_Parse(parser, szBuff, nRead, fIn.eof() ? 1 : 0)) {
			createError(XMLERR_FATAL, &stData, "XML parsing: %s",
						XML_ErrorString(XML_GetErrorCode(parser)));
			break;
		}
	}

	// print the end results
	printf("\t%d bytes parsed, %d errors(s), %d warnings(s)\n",
			nLength, stData.m_nErrors, stData.m_nWarnings);
	printf("Closing input XML spec ... ");
	fIn.close();
	printf("Ok.\n");

	// clean up
	if (stData.m_nErrors) {
		if (stData.m_pSpec != NULL)
			delete stData.m_pSpec;
		pSpec = NULL;
	}
	else
		pSpec = stData.m_pSpec;

	// return number of errors
	return stData.m_nErrors;
}
