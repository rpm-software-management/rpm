// standard C++ includes
#include <string>

// our includes
#include "XMLPackage.h"
#include "XMLSpec.h"

// attribute structure for XMLPackage
structValidAttrs g_paPackageAttrs[] =
{
	{0x0000,    false, false, "name"},
	{0x0001,    false, false, "group"},
	{0x0002,    false, false, "sub"},
	{XATTR_END, false, false, "end"}
};

bool XMLPackage::parseCreate(XMLAttrs* pAttrs,
							 XMLSpec* pSpec)
{
	// validate the attributes
	if (!pAttrs->validate(g_paPackageAttrs, (XMLBase*)pSpec))
		return false;

	// setup the name attribute
	string sName;
	if (pAttrs->get("name"))
		sName.assign(pAttrs->get("name"));

	// is this something else but a sub-package
	bool bSub = true;
	if (pAttrs->get("sub"))
		if (strcasecmp(pAttrs->get("sub"), "no") == 0)
			bSub = false;

	// if we have a name, cool, now test if the package already exists
	if (sName.length()) {
		XMLPackage package(sName.c_str(),
						   pAttrs->get("group"),
						   bSub);
		pSpec->addPackage(package);
	}

	// already something existing with %{name} ?
	else {
		XMLPackage package(NULL,
						   pAttrs->get("group"),
						   bSub);
		pSpec->addPackage(package);
	}
	return true;
}

bool XMLPackage::setDescription(const char* szDescription,
								XMLSpec* pSpec)
{
	if (pSpec) {
		pSpec->lastPackage().setDescription(szDescription);
		return true;
	}
	else
		return false;
}

bool XMLPackage::setSummary(const char* szSummary,
							XMLSpec* pSpec)
{
	if (pSpec) {
		pSpec->lastPackage().setSummary(szSummary);
		return true;
	}
	else
		return false;
}

XMLPackage::XMLPackage(const char* szName,
					   const char* szGroup,
					   bool bSub)
	: XMLBase()
{
	if (szName)
		m_sName.assign(szName);
	if (szGroup)
		m_sGroup.assign(szGroup);
	m_bSub = bSub;
}

XMLPackage::XMLPackage(const XMLPackage& rPackage)
	: XMLBase()
{
	m_sName.assign(rPackage.m_sName);
	m_sGroup.assign(rPackage.m_sGroup);
	m_sSummary.assign(rPackage.m_sSummary);
	m_sDescription.assign(rPackage.m_sDescription);
	m_bSub = rPackage.m_bSub;
	m_Requires = rPackage.m_Requires;
	m_BuildRequires = rPackage.m_BuildRequires;
	m_Provides = rPackage.m_Provides;
	m_Obsoletes = rPackage.m_Obsoletes;
	m_Post = rPackage.m_Post;
	m_PostUn = rPackage.m_PostUn;
	m_Pre = rPackage.m_Pre;
	m_PreUn = rPackage.m_PreUn;
	m_Verify = rPackage.m_Verify;
	m_Files = rPackage.m_Files;
}

XMLPackage::~XMLPackage()
{
}

void XMLPackage::toSpecFile(ostream& rOut)
{
	// top package bit
	if (hasName()) {
		rOut << endl << "%package";
		rOut << (!isSubPackage() ? " -n " : " ") << getName() << endl;
	}
	else
		rOut << endl << endl;

	// add the summary
	if (hasSummary())
		rOut << "summary:        " << getSummary() << endl;

	// do we have a group?
	if (hasGroup())
		rOut << "group:          " << getGroup() << endl;

	getProvides().toSpecFile(rOut, "provides");
	getObsoletes().toSpecFile(rOut, "obsoletes");
	getRequires().toSpecFile(rOut, "requires");
	getBuildRequires().toSpecFile(rOut, "buildrequires");

	// add the description
	if (hasDescription()) {
		rOut << "%description ";
		if (hasName()) {
			rOut << (!isSubPackage() ? "-n " : "");
			rOut << getName();
		}
		rOut << endl << getDescription() << endl;
	}
}

void toSectionSpecFile(ostream& rOut,
					   const char* szSection,
					   XMLPackage* pPkg)
{
	rOut << endl << "%" << szSection;
	if (pPkg->hasName())
		rOut << (!pPkg->isSubPackage() ? " -n " : " ") << pPkg->getName();
	rOut << endl;
}

void XMLPackage::toScriptsSpecFile(ostream& rOut)
{
	if (getPre().numScripts()) {
		toSectionSpecFile(rOut, "pre", this);
		getPre().toSpecFile(rOut, "pre");
	}

	if (getPost().numScripts()) {
		toSectionSpecFile(rOut, "post", this);
		getPost().toSpecFile(rOut, "post");
	}

	if (getPreUn().numScripts()) {
		toSectionSpecFile(rOut, "preun", this);
		getPreUn().toSpecFile(rOut, "preun");
	}

	if (getPostUn().numScripts()) {
		toSectionSpecFile(rOut, "postun", this);
		getPostUn().toSpecFile(rOut, "postun");
	}

	if (getVerify().numScripts()) {
		toSectionSpecFile(rOut, "verifyscript", this);
		getVerify().toSpecFile(rOut, "verifyscript");
	}
}

void XMLPackage::toFilesSpecFile(ostream& rOut)
{
	rOut << endl << "%files";
	if (hasName())
		rOut << (!isSubPackage() ? " -n " : " ") << getName();
	rOut << endl;
	getFiles().toSpecFile(rOut);
}

void XMLPackage::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t<package";
	if (hasName()) {
		rOut << " name=\"" << getName() << "\"";
		if (!isSubPackage())
			rOut << " sub=\"no\"";
	}
	if (hasGroup())
		rOut << " group=\"" << getGroup() << "\"";
	rOut << ">";
	if (hasSummary())
		rOut << endl << "\t\t<summary>" << getSummary() << "\t\t</summary>";
	if (hasDescription())
		rOut << endl << "\t\t<description>" << getDescription() << "\t\t</description>";

	getProvides().toXMLFile(rOut, "provides");
	getObsoletes().toXMLFile(rOut, "obsoletes");
	getRequires().toXMLFile(rOut, "requires");
	getBuildRequires().toXMLFile(rOut, "buildrequires");

	getPre().toXMLFile(rOut, "pre");
	getPost().toXMLFile(rOut, "post");
	getPreUn().toXMLFile(rOut, "preun");
	getPostUn().toXMLFile(rOut, "postun");
	getVerify().toXMLFile(rOut, "verify");

	getFiles().toXMLFile(rOut);

	rOut << endl << "\t</package>";
}

void XMLPackage::toRPMStruct(Spec spec)
{
	//Package pkg = newPackage(spec);
	//if (!hasName() && pkg == spec->packages)
	//	fillOutMainPackage(pkg->header);
	//else if (pkg != spec->packages)
	//	headerCopyTags(spec->packages->header,
	//				   pkg->header,
	//				   (int_32 *)copyTagsDuringParse);
}
