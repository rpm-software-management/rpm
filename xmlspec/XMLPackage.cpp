// standard C++ includes
#include <string>

// our includes
#include "XMLPackage.h"
#include "XMLRPMWrap.h"
#include "XMLSpec.h"

// rpm includes
#include <rpmlib.h>

// attribute structure for XMLPackage
structValidAttrs g_paPackageAttrs[] =
{
	{0x0000,    false, false, "name",        XATTRTYPE_STRING, {"*", NULL}},
	{0x0001,    true,  false, "group",       XATTRTYPE_STRING, {"*", NULL}},
	{0x0002,    false, false, "autoreq",     XATTRTYPE_BOOL,   {NULL}},
	{0x0003,    false, false, "autoprov",    XATTRTYPE_BOOL,   {NULL}},
	{0x0004,    false, false, "autoreqprov", XATTRTYPE_BOOL,   {NULL}},
	{0x0005,    false, false, "sub",         XATTRTYPE_BOOL,   {NULL}},
	{XATTR_END, false, false, "end",         XATTRTYPE_NONE,   {NULL}}
};

bool XMLPackage::parseCreate(XMLAttrs* pAttrs,
							 XMLSpec* pSpec)
{
	// validate the attributes
	if (!pAttrs->validate(g_paPackageAttrs, (XMLBase*)pSpec))
		return false;

	// setup the name attribute
	string sName;
	if (pAttrs->asString("name"))
		sName.assign(pAttrs->asString("name"));

	// if we have a name, cool, now test if the package already exists
	if (sName.length()) {
		XMLPackage package(sName.c_str(), pAttrs->asString("group"),
						   pAttrs->asBool("autoreq") || pAttrs->asBool("autoreqprov"),
						   pAttrs->asBool("autoprov") || pAttrs->asBool("autoreqprov"),
						   pAttrs->asBool("sub"));
		pSpec->addPackage(package);
	}

	// already something existing with %{name} ?
	else {
		XMLPackage package(NULL, pAttrs->asString("group"),
						   pAttrs->asBool("autoreq") || pAttrs->asBool("autoreqprov"),
						   pAttrs->asBool("autoprov") || pAttrs->asBool("autoreqprov"),
						   pAttrs->asBool("sub"));
		pSpec->addPackage(package);
	}
	return true;
}

bool XMLPackage::structCreate(PackageStruct* pPackage,
							  Spec pSpec,
							  XMLSpec* pXSpec)
{
	if (!pXSpec || !pSpec || !pPackage || !pPackage->header)
		return false;

	string sGroup, sName;
	if (!getRPMHeader(pPackage->header, RPMTAG_GROUP, sGroup))
		return false;
	getRPMHeader(pPackage->header, RPMTAG_NAME, sName);
	bool bSub = false;
	if (sName.compare(pXSpec->getName()) == 0) {
		bSub = true;
	}

	XMLPackage package(bSub ? NULL : sName.c_str(), sGroup.c_str(),
					   pPackage->autoReq ? true : false,
					   pPackage->autoProv ? true : false,
					   bSub);
	t_StrVector svText;
	t_StrVector svLang;
	getRPMHeaderArray(pPackage->header, RPMTAG_HEADERI18NTABLE, svLang);
	if (getRPMHeaderArray(pPackage->header, RPMTAG_SUMMARY, svText)) {
		for (unsigned int i = 0; i < svText.size(); i++)
			package.addSummary(svText[i].c_str(), svLang[i].c_str());
	}
	if (getRPMHeaderArray(pPackage->header, RPMTAG_DESCRIPTION, svText)) {
		for (unsigned int i = 0; i < svText.size(); i++)
			package.addDescription(svText[i].c_str(), svLang[i].c_str());
	}
	pXSpec->addPackage(package);

	XMLPackageContainer::structCreate(pPackage, pSpec, pXSpec);
	XMLFiles::structCreate(pPackage, pSpec, pXSpec);

	// do the next package and return
	XMLPackage::structCreate(pPackage->next, pSpec, pXSpec);
	return true;
}

// attribute structure for summaries
structValidAttrs g_paDescriptionAttrs[] =
{
	{0x0000,    false, false, "lang", XATTRTYPE_STRING, {"*", NULL}},
	{XATTR_END, false, false, "end",  XATTRTYPE_NONE,   {NULL}}
};

bool XMLPackage::addDescription(XMLAttrs* pAttrs,
								const char* szDescription,
								XMLSpec* pSpec)
{
	if (pSpec && pAttrs->validate(g_paDescriptionAttrs, (XMLBase*)pSpec)) {
		pSpec->lastPackage().addDescription(szDescription, pAttrs->asString("lang"));
		return true;
	}
	else
		return false;
}

// attribute structure for summaries
structValidAttrs g_paSummaryAttrs[] =
{
	{0x0000,    false, false, "lang", XATTRTYPE_STRING, {"*", NULL}},
	{XATTR_END, false, false, "end",  XATTRTYPE_NONE,   {NULL}}
};

bool XMLPackage::addSummary(XMLAttrs* pAttrs,
							const char* szSummary,
							XMLSpec* pSpec)
{
	if (pSpec && pAttrs->validate(g_paSummaryAttrs, (XMLBase*)pSpec)) {
		pSpec->lastPackage().addSummary(szSummary, pAttrs->asString("lang"));
		return true;
	}
	else
		return false;
}

XMLPackage::XMLPackage(const char* szName,
					   const char* szGroup,
					   bool bAutoReq,
					   bool bAutoProv,
					   bool bSub)
	: XMLBase()
{
	setName(szName);
	setGroup(szGroup);
	setSubPackage(bSub);
	setAutoRequires(bAutoReq);
	setAutoProvides(bAutoProv);
}

XMLPackage::XMLPackage(const XMLPackage& rPackage)
	: XMLBase()
{
	setName(rPackage.m_sName.c_str());
	setGroup(rPackage.m_sGroup.c_str());
	setSubPackage(rPackage.m_bSub);
	setAutoRequires(rPackage.m_bAutoReq);
	setAutoProvides(rPackage.m_bAutoProv);
	m_vSummaries = rPackage.m_vSummaries;
	m_vDescriptions = rPackage.m_vDescriptions;
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

	for (unsigned int i = 0; i < numSummaries(); i++) {
		rOut << "summary";
		if (getSummary(i).hasLang())
			rOut << "(" << getSummary(i).getLang() << "):";
		else
			rOut << ":    ";
		rOut << "    " << getSummary(i).getText() << endl;
	}
	if (hasGroup())
		rOut << "group:          " << getGroup() << endl;
	if (!hasAutoRequires() && !hasAutoProvides())
		rOut << "autoreqprov:    no" << endl;
	else {
		if (!hasAutoRequires())
			rOut << "autoreq:        no" << endl;
		if (!hasAutoProvides())
			rOut << "autoprov:       no" << endl;
	}

	getProvides().toSpecFile(rOut, "provides");
	getObsoletes().toSpecFile(rOut, "obsoletes");
	getRequires().toSpecFile(rOut, "requires");
	getBuildRequires().toSpecFile(rOut, "buildrequires");

	// add the description
	for (unsigned int i = 0; i < numDescriptions(); i++) {
		rOut << "%description ";
		if (getDescription(i).hasLang())
			rOut << "-l " << getDescription(i).getLang() << " ";
		if (hasName()) {
			rOut << (!isSubPackage() ? "-n " : "");
			rOut << getName();
		}
		rOut << endl << getDescription(i).getText() << endl;
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
	if (!hasAutoRequires() && !hasAutoProvides())
		rOut << " autoreqprov=\"no\"";
	else {
		if (!hasAutoRequires())
			rOut << " autoreq=\"no\"";
		if (!hasAutoProvides())
			rOut << " autoprov=\"no\"";
	}
	rOut << ">";

	for (unsigned int i = 0; i < numSummaries(); i++) {
		rOut << endl << "\t\t<summary";
		if (getSummary(i).hasLang())
			rOut << " lang=\"" << getSummary(i).getLang() << "\"";
		rOut << ">" << getSummary(i).getText() << "</summary>";
	}
	for (unsigned int i = 0; i < numDescriptions(); i++) {
		rOut << endl << "\t\t<description";
		if (getDescription(i).hasLang())
			rOut << " lang=\"" << getDescription(i).getLang() << "\"";
		rOut << ">" << getDescription(i).getText() << "</description>";
	}

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
