// our includes
#include "XMLMirror.h"
#include "XMLSpec.h"

using namespace std;

// attribute structure for XMLMirror
structValidAttrs g_paMirrorAttrs[] =
{
	{0x0000,    true,  false, "path"},
	{0x0001,    false, false, "description"},
	{0x0002,    false, false, "country"},
	{XATTR_END, false, false, "end"}
};

bool XMLMirror::parseCreate(XMLAttrs* pAttrs,
							XMLSpec* pSpec,
							bool bPatch)
{
	// validate the attributes
	if (!pAttrs->validate(g_paMirrorAttrs, (XMLBase*)pSpec))
		return false;

	XMLMirror mirror(pAttrs->get("path"),
					 pAttrs->get("description"),
					 pAttrs->get("country"));
	if (bPatch && pSpec->numPatches())
		pSpec->lastPatch().addMirror(mirror);
	else if (!bPatch && pSpec->numSources())
		pSpec->lastSource().addMirror(mirror);
	return true;
}

XMLMirror::XMLMirror(const char* szPath,
					 const char* szDescription,
					 const char* szCountry) : XMLBase()
{
	if (szPath)
		m_sPath.assign(szPath);
	if (szDescription)
		m_sDescription.assign(szDescription);
	if (szCountry)
		m_sCountry.assign(szCountry);
}

XMLMirror::XMLMirror(const XMLMirror& rMirror)
	: XMLBase()
{
	m_sPath.assign(rMirror.m_sPath);
	m_sDescription.assign(rMirror.m_sDescription);
	m_sCountry.assign(rMirror.m_sCountry);
}

XMLMirror::~XMLMirror()
{
}

XMLMirror XMLMirror::operator=(XMLMirror mirror)
{
	m_sPath.assign(mirror.m_sPath);
	m_sDescription.assign(mirror.m_sDescription);
	m_sCountry.assign(mirror.m_sCountry);
}

void XMLMirror::toSpecFile(ostream& rOut)
{
	rOut << endl << "# mirror: " << getPath();
}

void XMLMirror::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t\t<mirror path=\"" << getPath() << "\"";
	if (hasDescription())
		rOut << endl << "\t\t        description=\"" << getDescription() << "\"";
	if (hasCountry())
		rOut << endl << "\t\t        country=\"" << getCountry() << "\"";
	rOut << " />";
}
