// standard includes
#include <stdio.h>

// our includes
#include "XMLSource.h"
#include "XMLSpec.h"

// rpm includes
#include <rpmspec.h>

using namespace std;

bool XMLSource::structCreate(Source* pSource,
							 XMLSpec* pSpec)
{
	if (!pSpec || !pSource)
		return false;

	do {
		// create our mirror
		XMLMirror *pMirror = NULL;
		if (pSource->source != pSource->fullSource) {
			unsigned int nLen = pSource->source-pSource->fullSource;
			char szPath[nLen+1];
			strncpy(szPath, pSource->fullSource, nLen);
			szPath[nLen] = '\0';
			pMirror = new XMLMirror(szPath, NULL, NULL);
		}

		// generate the source, nosource, patch
		XMLSource* pXSource = NULL;
		XMLNoSource* pXNoSource = NULL;
		XMLPatch* pXPatch = NULL;
		switch (pSource->flags) {
			case RPMBUILD_ISSOURCE:
    			pXSource = new XMLSource(pSource->source,
										 pSource->num,
										 NULL,
										 NULL,
										 NULL);
				pSpec->addSource(*pXSource);
				if (pMirror)
					pSpec->lastSource().addMirror(*pMirror);
				delete pXSource;
				break;
			case RPMBUILD_ISNO:
				pXNoSource = new XMLNoSource(pSource->source,
											 pSource->num,
											 NULL,
											 NULL,
											 NULL);
				pSpec->addNoSource(*pXNoSource);
				if (pMirror)
					pSpec->lastNoSource().addMirror(*pMirror);
				delete pXNoSource;
				break;
			case RPMBUILD_ISPATCH:
				pXPatch = new XMLPatch(pSource->source,
									   pSource->num,
									   NULL,
									   NULL);
				pSpec->addPatch(*pXPatch);
				if (pMirror)
					pSpec->lastPatch().addMirror(*pMirror);
				delete pXPatch;
				break;
			default:
				break;
		}
		if (pMirror)
			delete pMirror;
	} while ((pSource = pSource->next));
	return true;
}

// attribute structure for XMLSource
structValidAttrs g_paSourceAttrs[] =
{
	{0x0000,    true,  false, "name"},
	{0x0001,    false, false, "num"},
	{0x0002,    false, false, "dir"},
	{0x0003,    false, false, "size"},
	{0x0004,    false, false, "md5"},
	{XATTR_END, false, false, "end"}
};

bool XMLSource::parseCreate(XMLAttrs* pAttrs,
							XMLSpec* pSpec)
{
	// validate the attributes
	if (!pAttrs->validate(g_paSourceAttrs, (XMLBase*)pSpec))
		return false;

	// create and return
	unsigned int nNum = 0;
	if (pAttrs->get("num"))
		nNum = atoi(pAttrs->get("num"));
	XMLSource source(pAttrs->get("name"),
					 nNum,
					 pAttrs->get("dir"),
					 pAttrs->get("size"),
					 pAttrs->get("md5"));
	pSpec->addSource(source);

	return true;
}

XMLSource::XMLSource(const char* szName,
					 unsigned int nNum,
					 const char* szDir,
					 const char* szSize,
					 const char* szMD5)
	: XMLBase()
{
	if (szName)
		m_sName.assign(szName);
	m_nNum = nNum;
	if (szDir)
		m_sDir.assign(szDir);
	if (szSize)
		m_sSize.assign(szSize);
	if (szMD5)
		m_sMD5.assign(szMD5);
}

XMLSource::XMLSource(const XMLSource& rSource)
	: XMLBase()
{
	m_sName.assign(rSource.m_sName);
	m_nNum = rSource.m_nNum;
	m_sDir.assign(rSource.m_sDir);
	m_sSize.assign(rSource.m_sSize);
	m_sMD5.assign(rSource.m_sMD5);
	m_vMirrors = rSource.m_vMirrors;
}

XMLSource::~XMLSource()
{
}

XMLSource XMLSource::operator=(XMLSource source)
{
	m_sName.assign(source.m_sName);
	m_nNum = source.m_nNum;
	m_sDir.assign(source.m_sDir);
	m_sSize.assign(source.m_sSize);
	m_sMD5.assign(source.m_sMD5);
	m_vMirrors = source.m_vMirrors;
	return *this;
}

void XMLSource::toSpecFile(ostream& rOut)
{
	for (unsigned int i = 0; i < numMirrors(); i++)
		getMirror(i).toSpecFile(rOut);

	rOut << endl << "source";
	rOut << getNum();
	rOut << ":        ";
	if (numMirrors())
		rOut << getMirror(0).getPath();
	rOut << getName();
}

void XMLSource::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t<source name=\"" << getName() << "\"";
	rOut << endl << "\t        num=\"";
	rOut << getNum();
	rOut << "\"";
	if (hasSize())
		rOut << endl << "\t        size=\"" << getSize() << "\"";
	if (hasMD5())
		rOut << endl << "\t        md5=\"" << getMD5() << "\"";
	if (hasDir())
		rOut << endl << "\t        dir=\"" << getDir() << "\"";
	rOut << ">";

	for (unsigned int i = 0; i < numMirrors(); i++)
		getMirror(i).toXMLFile(rOut);

	rOut << endl << "\t</source>";
}

void XMLSource::toRPMStruct(Spec pRPMSpec)
{
	/*Source* pCurr = new Source;
	unsigned int nNameLen = strlen(getName());
	unsigned int nMirrorLen = 0;
	if (pPrev)
		pPrev->next = pCurr;
	pCurr->next = NULL;
	if (numMirrors())
		nMirrorlen = strlen(getMirror(0).getPath());
	pCurr->fullSource = new char[nNamelen+nMirrorLen+1];
	pCurr->fullSources[0] = '\0';
	if (numMirrors())
		strcpy(pCurr->fullSource, getMirror(0).getPath());
	strcat(pCurr->fullSource, getName());
	pCurr->source = pCurr->fullSource+nMirrorLen;
	pCurr->num = getNum();
	pCurr->flags = RPMBUILD_ISSOURCE;
	return pCurr;*/
}

bool XMLNoSource::parseCreate(XMLAttrs* pAttrs,
							XMLSpec* pSpec)
{
	// validate the attributes
	if (!pAttrs->validate(g_paSourceAttrs, (XMLBase*)pSpec))
		return false;

	unsigned int nNum = 0;
	if (pAttrs->get("num"))
		nNum = atoi(pAttrs->get("num"));
	XMLNoSource source(pAttrs->get("name"),
					   nNum,
					   pAttrs->get("dir"),
					   pAttrs->get("size"),
					   pAttrs->get("md5"));
	pSpec->addNoSource(source);
	return true;
}

XMLNoSource::XMLNoSource(const char* szName,
						 unsigned int nNum,
						 const char* szDir,
						 const char* szSize,
						 const char* szMD5)
	: XMLSource(szName,
				nNum,
				szDir,
				szSize,
				szMD5)
{
}

XMLNoSource::XMLNoSource(const XMLNoSource& rNoSource)
	: XMLSource(rNoSource.m_sName.c_str(),
				rNoSource.m_nNum,
				rNoSource.m_sDir.c_str(),
				rNoSource.m_sSize.c_str(),
				rNoSource.m_sMD5.c_str())
{
}

XMLNoSource::~XMLNoSource()
{
}

void XMLNoSource::toSpecFile(ostream& rOut)
{
	for (unsigned int i = 0; i < numMirrors(); i++)
		getMirror(i).toSpecFile(rOut);

	rOut << endl << "nosource";
	rOut << getNum();
	rOut << ":      " << getName();
}

void XMLNoSource::toXMLFile(ostream& rOut)
{
	for (unsigned int i = 0; i < numMirrors(); i++)
		getMirror(i).toXMLFile(rOut);
}

void XMLNoSource::toRPMStruct(Spec pRPMSpec)
{
	//Source* pCurr = XMLSource::toRPMStruct(pPrev);
	//pCurr->flags = RPMBUILD_ISNO;
	//return pCurr;
}

// attribute structure for XMLPatch
structValidAttrs g_paPatchAttrs[] =
{
	{0x0000,    true,  false, "name"},
	{0x0001,    false, false, "num"},
	{0x0002,    false, false, "size"},
	{0x0003,    false, false, "md5"},
	{XATTR_END, false, false, "end"}
};

bool XMLPatch::parseCreate(XMLAttrs* pAttrs,
						   XMLSpec* pSpec)
{
	// validate the attributes
	if (!pAttrs->validate(g_paPatchAttrs, (XMLBase*)pSpec))
		return false;

	unsigned int nNum = 0;
	if (pAttrs->get("num"))
		nNum = atoi(pAttrs->get("num"));
	XMLPatch patch(pAttrs->get("name"),
				   nNum,
				   pAttrs->get("size"),
				   pAttrs->get("md5"));
	pSpec->addPatch(patch);
	return true;
}

XMLPatch::XMLPatch(const char* szName,
				   unsigned int nNum,
				   const char* szSize,
				   const char* szMD5)
	: XMLSource(szName,
				nNum,
				NULL,
				szSize,
				szMD5)
{
}

XMLPatch::XMLPatch(const XMLPatch& rPatch)
	: XMLSource(rPatch.m_sName.c_str(),
				rPatch.m_nNum,
				NULL,
				rPatch.m_sSize.c_str(),
				rPatch.m_sMD5.c_str())
{
}

XMLPatch::~XMLPatch()
{
}

void XMLPatch::toSpecFile(ostream& rOut)
{
	for (unsigned int i = 0; i < numMirrors(); i++)
		getMirror(i).toSpecFile(rOut);
	rOut << endl << "patch";
	rOut << getNum();
	rOut << ":         " << getName();
}

void XMLPatch::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t<patch name=\"" << getName() << "\"";
	rOut << endl << "\t       num=\"";
	rOut << getNum();
	rOut << "\"";
	if (hasSize())
		rOut << endl << "\t       size=\"" << getSize() << "\"";
	if (hasMD5())
		rOut << endl << "\t       md5=\"" << getMD5() << "\"";
	if (hasDir())
		rOut << endl << "\t       dir=\"" << getDir() << "\"";
	rOut << ">";

	for (unsigned int i = 0; i < numMirrors(); i++)
		getMirror(i).toXMLFile(rOut);

	rOut << endl << "\t</patch>";
}

void XMLPatch::toRPMStruct(Spec pRPMSpec)
{

	//Source* pCurr = XMLSource::toRPMStruct(pPrev);
	//pCurr->flags = RPMBUILD_ISPATCH;
	//return pCurr;
}
