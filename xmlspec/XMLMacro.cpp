// our includes
#include "XMLMacro.h"
#include "XMLSpec.h"

using namespace std;

// attribute structure for XMLMacro
structValidAttrs g_paMacroAttrs[] =
{
	{0x0000,    true,  false, "name", XATTRTYPE_STRING, {"*", NULL}},
	{XATTR_END, false, false, "end",  XATTRTYPE_NONE,   {NULL}}
};

bool XMLMacro::parseCreate(XMLAttrs* pAttrs,
						   const char* szMacro,
						   XMLSpec* pSpec)
{
	if (!pSpec || !szMacro || !pAttrs->validate(g_paMacroAttrs, (XMLBase*)pAttrs))
		return false;
	XMLMacro macro(pAttrs->asString("name"), szMacro);
	pSpec->addXMacro(macro);
	return true;
}

XMLMacro::XMLMacro(const char* szName,
				   const char* szMacro)
	: XMLBase()
{
	if (szName)
		m_sName.assign(szName);
	if (szMacro)
		m_sValue.assign(szMacro);
}

XMLMacro::XMLMacro(const XMLMacro& rMacro)
	: XMLBase()
{
	m_sName.assign(rMacro.m_sName);
	m_sValue.assign(rMacro.m_sValue);
}

XMLMacro::~XMLMacro()
{
}

XMLMacro XMLMacro::operator=(XMLMacro macro)
{
	m_sName.assign(macro.m_sName);
	m_sValue.assign(macro.m_sValue);
}

void XMLMacro::toSpecFile(ostream& rOut)
{
	rOut << "%define " << getName() << " " << getValue() << endl;
}

void XMLMacro::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t<macro name=\"" << getName() << "\">";
	rOut << getValue() << "</macro>";
}
