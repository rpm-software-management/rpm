// standard includes
#include <stdio.h>

// our includes
#include "XMLAttrs.h"
#include "XMLFiles.h"
#include "XMLPackage.h"
#include "XMLSpec.h"

// rpm includes
#include <rpmlib.h>
#include <stringbuf.h>

using namespace std;

// attribute structure for XMLFile
structValidAttrs g_paFileAttrs[] =
{
	{0x0000,    false, false, "mode",   XATTRTYPE_INTEGER, {NULL}},
	{0x0001,    false, false, "group",  XATTRTYPE_STRING,  {"*", NULL}},
	{0x0002,    false, false, "user",   XATTRTYPE_STRING,  {"*", NULL}},
	{0x0003,    false, false, "config", XATTRTYPE_STRING,  {"noreplace",
															"*", NULL}},
	{XATTR_END, false, false, "end",    XATTRTYPE_NONE,    {NULL}}
};

bool XMLFile::parseCreate(XMLAttrs* pAttrs,
						  const char* szPath,
						  XMLSpec* pSpec)
{
	// validate the attributes
	if (!pSpec || !pAttrs->validate(g_paFileAttrs, (XMLBase*)pSpec))
		return false;

	// create and return
	XMLFile file(pAttrs->asString("mode"), pAttrs->asString("user"),
				 pAttrs->asString("group"), pAttrs->asString("config"), szPath);
	pSpec->lastPackage().getFiles().addFile(file);
	return true;
}

XMLFile::XMLFile(const char* szMode,
				 const char* szOwner,
				 const char* szGroup,
				 const char* szConfig,
				 const char* szPath)
	: XMLBase()
{
	if (szMode)
		m_sMode.assign(szMode);
	if (szOwner)
		m_sOwner.assign(szOwner);
	if (szGroup)
		m_sGroup.assign(szGroup);
	if (szConfig)
		m_sConfig.assign(szConfig);
	if (szPath)
		m_sPath.assign(szPath);
}

XMLFile::XMLFile(const XMLFile& rFile)
	: XMLBase()
{
	setMode(rFile.m_sMode.c_str());
	setOwner(rFile.m_sOwner.c_str());
	setGroup(rFile.m_sGroup.c_str());
	setConfig(rFile.m_sConfig.c_str());
	setPath(rFile.m_sPath.c_str());
}

XMLFile::~XMLFile()
{
}

XMLFile XMLFile::operator=(XMLFile file)
{
	setMode(file.m_sMode.c_str());
	setOwner(file.m_sOwner.c_str());
	setGroup(file.m_sGroup.c_str());
	setConfig(file.m_sConfig.c_str());
	setPath(file.m_sPath.c_str());
}

void XMLFile::toSpecFile(ostream& rOut)
{
	if (hasMode() || hasOwner() || hasGroup()) {
		rOut << "%attr(";
		rOut << (hasMode() ? getMode() : "-");
		rOut << "," << (hasOwner() ? getOwner() : "-");
		rOut << "," << (hasGroup() ? getGroup() : "-");
		rOut << ") ";
	}
	if (hasConfig()) {
		rOut << "%config(" << getConfig() << ") ";
	}
	rOut << getPath() << endl;
}

void XMLFile::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t\t\t<file";
	if (hasMode())
		rOut << " mode=\"" << getMode() << "\"";
	if (hasOwner())
		rOut << " user=\"" << getOwner() << "\"";
	if (hasGroup())
		rOut << " group=\"" << getGroup() << "\"";
	if (hasConfig())
		rOut << " config=\"" << getConfig() << "\"";
	rOut << ">";
	rOut << getPath() << "</file>";
}

// attribute structure for XMLFiles
structValidAttrs g_paFilesAttrs[] =
{
	{0x0000,    false, false, "mode",  XATTRTYPE_INTEGER, {NULL}},
	{0x0001,    false, false, "group", XATTRTYPE_STRING,  {"*", NULL}},
	{0x0002,    false, false, "user",  XATTRTYPE_STRING,  {"*", NULL}},
	{XATTR_END, false, false, "end",   XATTRTYPE_NONE,    {NULL}}
};

bool XMLFiles::parseCreate(XMLAttrs* pAttrs,
						   XMLSpec* pSpec)
{
	if (!pSpec || !pAttrs->validate(g_paFilesAttrs, (XMLBase*)pSpec))
		return false;
	pSpec->lastPackage().getFiles().setDefMode(pAttrs->asString("mode"));
	pSpec->lastPackage().getFiles().setDefOwner(pAttrs->asString("user"));
	pSpec->lastPackage().getFiles().setDefGroup(pAttrs->asString("group"));
	return true;
}

bool XMLFiles::structCreate(PackageStruct* pPackage,
							Spec pSpec,
							XMLSpec* pXSpec)
{
	if (!pXSpec || !pSpec || !pPackage || !pPackage->fileList)
		return false;
	return true;
}

XMLFiles::XMLFiles()
	: XMLBase()
{
}

XMLFiles::XMLFiles(const XMLFiles& rFiles)
	: XMLBase()
{
	setDefMode(rFiles.m_sMode.c_str());
	setDefOwner(rFiles.m_sOwner.c_str());
	setDefGroup(rFiles.m_sGroup.c_str());
	m_vFiles = rFiles.m_vFiles;
}

XMLFiles::~XMLFiles()
{
}

XMLFiles XMLFiles::operator=(XMLFiles files)
{
	setDefMode(files.m_sMode.c_str());
	setDefOwner(files.m_sOwner.c_str());
	setDefGroup(files.m_sGroup.c_str());
	m_vFiles = files.m_vFiles;
	return *this;
}

void XMLFiles::toSpecFile(ostream& rOut)
{
	if (numFiles()) {
		if (hasDefMode() || hasDefOwner() || hasDefGroup()) {
			rOut << "%defattr(";
			rOut << (hasDefMode() ? getDefMode() : "-");
			rOut << "," << (hasDefOwner() ? getDefOwner() : "-");
			rOut << "," << (hasDefGroup() ? getDefGroup() : "-");
			rOut << ")" << endl;
		}
		for (unsigned int i = 0; i < numFiles(); i++)
			getFile(i).toSpecFile(rOut);
	}
}

void XMLFiles::toXMLFile(ostream& rOut)
{
	if (numFiles()) {
		rOut << endl << "\t\t<files";
		if (hasDefMode())
			rOut << " mode=\"" << getDefMode() << "\"";
		if (hasDefOwner())
			rOut << " user=\"" << getDefOwner() << "\"";
		if (hasDefGroup())
			rOut << " group=\"" << getDefGroup() << "\"";
		rOut << ">";
		for (unsigned int i = 0; i < numFiles(); i++)
			getFile(i).toXMLFile(rOut);
		rOut << endl << "\t\t</files>";
	}
}
