// our includes
#include "XMLAttrs.h"
#include "XMLPackage.h"
#include "XMLRequires.h"
#include "XMLRPMWrap.h"
#include "XMLSpec.h"

// rpm includes
#include <rpmlib.h>

using namespace std;

// attribute structure for XMLPackageEntry
structValidAttrs g_paEntryAttrs[] =
{
	{0x0000,    true,  false, "name",    XATTRTYPE_STRING, {"*", NULL}},
	{0x0001,    false, false, "version", XATTRTYPE_STRING, {"*", NULL}},
	{0x0002,    false, false, "cmp",     XATTRTYPE_STRING, {"*", NULL}},
	{XATTR_END, false, false, "end",     XATTRTYPE_NONE,   {NULL}}
};

bool XMLPackageEntry::parseCreate(XMLAttrs* pAttrs,
								  XMLPackageContainer& rContainer)
{
	// validate the attributes
	if (!pAttrs->validate(g_paEntryAttrs, (XMLBase*)pAttrs))
		return false;

	// create and return
	XMLPackageEntry entry(pAttrs->asString("name"), pAttrs->asString("version"),
						  pAttrs->asString("cmp"));
	rContainer.addEntry(entry);
	return true;
}

XMLPackageEntry::XMLPackageEntry(const char* szName,
								 const char* szVersion,
								 const char* szCmp)
	: XMLBase()
{
	if (szName)
		m_sName.assign(szName);
	if (szVersion)
		m_sVersion.assign(szVersion);
	m_sCmp.assign("=");
	if (szCmp) {
		if (strcasecmp(szCmp, "lt") == 0)
			m_sCmp.assign("<");
		else if (strcasecmp(szCmp, "le") == 0)
			m_sCmp.assign("<=");
		else if (strcasecmp(szCmp, "gt") == 0)
			m_sCmp.assign(">");
		else if (strcasecmp(szCmp, "ge") == 0)
			m_sCmp.assign(">=");
	}
}

XMLPackageEntry::XMLPackageEntry(const XMLPackageEntry& rEntry)
	: XMLBase()
{
	m_sName.assign(rEntry.m_sName);
	m_sVersion.assign(rEntry.m_sVersion);
	m_sCmp.assign(rEntry.m_sCmp);
}

XMLPackageEntry::~XMLPackageEntry()
{
}

XMLPackageEntry XMLPackageEntry::operator=(XMLPackageEntry entry)
{
	m_sName.assign(entry.m_sName);
	m_sVersion.assign(entry.m_sVersion);
	m_sCmp.assign(entry.m_sCmp);
	return *this;
}

void XMLPackageEntry::toSpecFile(ostream& rOut)
{
	rOut << getName();
	if (hasVersion()) {
		rOut << " " << getCompare() << " " << getVersion();
	}
}

void XMLPackageEntry::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t\t\t<package name=\"" << getName() << "\"";
	if (hasVersion()) {
		if (m_sCmp.compare("=") == 0)
			rOut << " cmp=\"eq\"";
		else if (m_sCmp.compare("<") == 0)
			rOut << " cmp=\"lt\"";
		else if (m_sCmp.compare("<=") == 0)
			rOut << " cmp=\"le\"";
		else if (m_sCmp.compare(">") == 0)
			rOut << " cmp=\"gt\"";
		else if (m_sCmp.compare(">=") == 0)
			rOut << " cmp=\"ge\"";
		rOut << " version=\"" << getVersion() << "\"";
	}
	rOut << " />";
}

XMLPackageContainer::XMLPackageContainer()
	: XMLBase()
{
}

XMLPackageContainer::XMLPackageContainer(const XMLPackageContainer& rContainer)
	: XMLBase()
{
	m_vPackages = rContainer.m_vPackages;
}

XMLPackageContainer::~XMLPackageContainer()
{
}

XMLPackageContainer XMLPackageContainer::operator=(XMLPackageContainer container)
{
	m_vPackages = container.m_vPackages;
	return *this;
}

void XMLPackageContainer::toSpecFile(ostream& rOut,
									 const char* szTag)
{
	if (numEntries()) {
		rOut << szTag << ": ";
		for (unsigned int i = 0; i < numEntries(); i++) {
			rOut << (i ? ", " : "");
			getEntry(i).toSpecFile(rOut);
		}
		rOut << endl;
	}
}

void XMLPackageContainer::toXMLFile(ostream& rOut,
									const char* szTag)
{
	if (numEntries()) {
		rOut << endl << "\t\t<" << szTag << ">";
		for (unsigned int i = 0; i < numEntries(); i++)
			getEntry(i).toXMLFile(rOut);
		rOut << endl << "\t\t</" << szTag << ">";
	}
}

bool XMLPackageContainer::addRequire(XMLAttrs* pAttrs,
									 XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLPackageEntry::parseCreate(pAttrs, pSpec->lastPackage().getRequires());
}

bool XMLPackageContainer::addBuildRequire(XMLAttrs* pAttrs,
										  XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLPackageEntry::parseCreate(pAttrs, pSpec->lastPackage().getBuildRequires());
}

bool XMLPackageContainer::addProvide(XMLAttrs* pAttrs,
									 XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLPackageEntry::parseCreate(pAttrs, pSpec->lastPackage().getProvides());
}

bool XMLPackageContainer::addObsolete(XMLAttrs* pAttrs,
									  XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLPackageEntry::parseCreate(pAttrs, pSpec->lastPackage().getObsoletes());
}

bool XMLPackageContainer::structCreate(PackageStruct* pPackage,
									   Spec pSpec,
									   XMLSpec* pXSpec)
{
	if (!pXSpec || !pPackage || !pPackage->header)
		return false;
	return true;
}
