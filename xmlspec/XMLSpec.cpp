// 3rd party includes
#include <expat.h>

// our includes
#include "XMLAttrs.h"
#include "XMLSpec.h"

using namespace std;

// attribute structure for XMLSpec
structValidAttrs g_paSpecAttrs[] =
{
	{0x0000,    true,  false, "name"},
	{0x0001,    true,  false, "version"},
	{0x0002,    true,  false, "release"},
	{0x0003,    false, false, "epoch"},
	{0x0004,    false, false, "distribution"},
	{0x0005,    false, false, "vendor"},
	{0x0006,    false, false, "packager"},
	{0x0007,    false, false, "packager-email"},
	{0x0008,    false, false, "copyright"},
	{0x0009,    false, false, "url"},
	{0x000A,    false, false, "buildroot"},
	{XATTR_END, false, false, "end"}
};

XMLSpec* XMLSpec::parseCreate(XMLAttrs* pAttrs,
							  const char* szFilename)
{
	// verify the attributes
	if (!pAttrs->validate(g_paSpecAttrs, (XMLBase*)pAttrs))
		return NULL;

	// create and return
	return new XMLSpec(szFilename,
					   pAttrs->get("name"),
					   pAttrs->get("version"),
					   pAttrs->get("release"),
					   pAttrs->get("epoch"),
					   pAttrs->get("distribution"),
					   pAttrs->get("vendor"),
					   pAttrs->get("packager"),
					   pAttrs->get("packager-email"),
					   pAttrs->get("copyright"),
					   pAttrs->get("url"),
					   pAttrs->get("buildroot"));
}

XMLSpec* XMLSpec::structCreate(Spec spec)
{
	if (!spec)
		return NULL;

	XMLSpec* pSpec = new XMLSpec(spec->specFile,
								 spec->specFile,
								 "1.0",
								 "1",
								 "0",
								 NULL,
								 NULL,
								 NULL,
								 NULL,
								 NULL,
								 NULL,
								 spec->buildRootURL);

	XMLSource::structCreate(spec->sources,
							pSpec);
	return pSpec;
}

XMLSpec::XMLSpec(const char* szFilename,
				 const char* szName,
				 const char* szVersion,
				 const char* szRelease,
				 const char* szEpoch,
				 const char* szDistribution,
				 const char* szVendor,
				 const char* szPackager,
				 const char* szPkgrEmail,
				 const char* szCopyright,
				 const char* szURL,
				 const char* szBuildRoot) : XMLBase()
{
	if (szFilename)
		m_sFilename.assign(szFilename);
	if (szName)
		m_sName.assign(szName);
	if (szVersion)
		m_sVersion.assign(szVersion);
	if (szRelease)
		m_sRelease.assign(szRelease);
	if (szEpoch)
		m_sEpoch.assign(szEpoch);
	if (szDistribution)
		m_sDistribution.assign(szDistribution);
	if (szVendor)
		m_sVendor.assign(szVendor);
	if (szPackager)
		m_sPackager.assign(szPackager);
	if (szPkgrEmail)
		m_sPkgrEmail.assign(szPkgrEmail);
	if (szCopyright)
		m_sCopyright.assign(szCopyright);
	if (szURL)
		m_sURL.assign(szURL);
	if (szBuildRoot)
		m_sBuildRoot.assign(szBuildRoot);
}

XMLSpec::~XMLSpec()
{
}

void XMLSpec::toSpecFile(ostream& rOut)
{
	for (unsigned int i = 0; i < numXMacros(); i++)
		getXMacro(i).toSpecFile(rOut);

	rOut << "name:           " << getName() << endl;
	rOut << "version:        " << getVersion() << endl;
	rOut << "release:        " << getRelease() << endl;
	if (hasEpoch())
		rOut << "epoch:          " << getEpoch() << endl;
	if (hasCopyright())
		rOut << "copyright:      " << getCopyright() << endl;
	if (hasURL())
		rOut << "url:            " << getURL() << endl;
	if (hasBuildRoot())
		rOut << "buildroot:      " << getBuildRoot() << endl;
	if (hasDistribution())
		rOut << "distribution:   " << getDistribution() << endl;
	if (hasVendor())
		rOut << "vendor:         " << getVendor() << endl;
	if (hasPackager()) {
		rOut << "packager:       " << getPackager();
		if (hasPkgrEmail())
			rOut << " <" << getPkgrEmail() << ">";
		rOut << endl;
	}

	for (unsigned int i = 0; i < numSources(); i++)
		getSource(i).toSpecFile(rOut);
	for (unsigned int i = 0; i < numNoSources(); i++)
		getNoSource(i).toSpecFile(rOut);
	for (unsigned int i = 0; i < numPatches(); i++)
		getPatch(i).toSpecFile(rOut);
	for (unsigned int i = 0; i < numPackages(); i++)
		getPackage(i).toSpecFile(rOut);

	getPrep().toSpecFile(rOut, "prep");
	getBuild().toSpecFile(rOut, "build");
	getInstall().toSpecFile(rOut, "install");
	getClean().toSpecFile(rOut, "clean");

	for (unsigned int i = 0; i < numPackages(); i++)
		getPackage(i).toScriptsSpecFile(rOut);

	for (unsigned int i = 0; i < numPackages(); i++)
		getPackage(i).toFilesSpecFile(rOut);

	getChangelog().toSpecFile(rOut);
}

void XMLSpec::toXMLFile(ostream& rOut)
{
	// spec start
	rOut << "<?xml version=\"1.0\"?>";
	rOut << endl << "<spec name=\"" << getName() << "\"";
	rOut << endl << "      version=\"" << getVersion() << "\"";
	rOut << endl << "      release=\"" << getRelease() << "\"";
	if (hasEpoch())
		rOut << endl << "      epoch=\"" << getEpoch() << "\"";
	if (hasCopyright())
		rOut << endl << "      copyright=\"" << getCopyright() << "\"";
	if (hasURL())
		rOut << endl << "      url=\"" << getURL() << "\"";
	if (hasBuildRoot())
		rOut << endl << "      buildroot=\"" << getBuildRoot() << "\"";
	if (hasDistribution())
		rOut << endl << "      distribution=\"" << getDistribution() << "\"";
	if (hasVendor())
		rOut << endl << "      vendor=\"" << getVendor() << "\"";
	if (hasPackager()) {
		rOut << endl << "      packager=\"" << getPackager() << "\"";
		if (hasPkgrEmail())
			rOut << endl << "      packager-email=\"" << getPkgrEmail() << "\"";
	}
	rOut << ">";

	for (unsigned int i = 0; i < numXMacros(); i++)
		getXMacro(i).toXMLFile(rOut);
	for (unsigned int i = 0; i < numSources(); i++)
		getSource(i).toXMLFile(rOut);
	for (unsigned int i = 0; i < numNoSources(); i++)
		getNoSource(i).toXMLFile(rOut);
	for (unsigned int i = 0; i < numPatches(); i++)
		getPatch(i).toXMLFile(rOut);
	for (unsigned int i = 0; i < numPackages(); i++)
		getPackage(i).toXMLFile(rOut);

	getPrep().toXMLFile(rOut, "prep");
	getBuild().toXMLFile(rOut, "build");
	getInstall().toXMLFile(rOut, "install");
	getClean().toXMLFile(rOut, "clean");

	getChangelog().toXMLFile(rOut);

	rOut << endl << "</spec>";
}

void XMLSpec::toRPMStruct(Spec* pRPMSpec)
{
	Spec spec = newSpec();
	if (hasBuildRoot()) {
		spec->gotBuildRootURL = 1;
		spec->buildRootURL = strdup(getBuildRoot());
		addMacro(spec->macros, "buildroot", NULL, getBuildRoot(), RMIL_SPEC);
	}
	addMacro(NULL, "_docdir", NULL, "%{_defaultdocdir}", RMIL_SPEC);
	spec->timeCheck = rpmExpandNumeric("%{_timecheck}");

	//getChangelog().toRPMStruct(spec);
	/*for (unsigned int i = 0; i < numPackages(); i++)
		getPackage(i).toRPMStruct(pRPMSpec);
	for (unsigned int i = 0; i < numSources(); i++)
		getSource(i).toRPMStruct(pRPMSpec);
	for (unsigned int i = 0; i < numNoSources(); i++)
		getNoSource(i).toRPMStruct(pRPMSpec);
	for (unsigned int i = 0; i < numPatches(); i++)
		getPatch(i).toRPMStruct(pRPMSpec);*/

	//getPrep().toRPMStruct(spec);
	//getBuild().toRPMStruct(spec);
	//getInstall().toRPMStruct(spec);
	//getClean().toRPMStruct(spec);

	*pRPMSpec = spec;
}
