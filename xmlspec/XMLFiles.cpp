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
	{0x0000,    false, false, "attr",   XATTRTYPE_INTEGER, {NULL}},
	{0x0001,    false, false, "gid",    XATTRTYPE_STRING,  {"*", NULL}},
	{0x0002,    false, false, "uid",    XATTRTYPE_STRING,  {"*", NULL}},
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
	XMLFile file(pAttrs->asString("attr"), pAttrs->asString("uid"),
				 pAttrs->asString("gid"), pAttrs->asString("config"), szPath);
	pSpec->lastPackage().getFiles().addFile(file);
	return true;
}

XMLFile::XMLFile(const char* szAttr,
				 const char* szOwner,
				 const char* szGroup,
				 const char* szConfig,
				 const char* szPath)
	: XMLBase()
{
	if (szAttr)
		m_sAttr.assign(szAttr);
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
	m_sAttr.assign(rFile.m_sAttr);
	m_sOwner.assign(rFile.m_sOwner);
	m_sGroup.assign(rFile.m_sGroup);
	m_sConfig.assign(rFile.m_sConfig);
	m_sPath.assign(rFile.m_sPath);
}

XMLFile::~XMLFile()
{
}

XMLFile XMLFile::operator=(XMLFile file)
{
	m_sAttr.assign(file.m_sAttr);
	m_sOwner.assign(file.m_sOwner);
	m_sGroup.assign(file.m_sGroup);
	m_sConfig.assign(file.m_sConfig);
	m_sPath.assign(file.m_sPath);
}

void XMLFile::toSpecFile(ostream& rOut)
{
	if (hasAttr() || hasOwner() || hasGroup()) {
		rOut << "%attr(";
		rOut << (hasAttr() ? getAttr() : "-");
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
	if (hasAttr())
		rOut << " attr=\"" << getAttr() << "\"";
	if (hasOwner())
		rOut << " uid=\"" << getOwner() << "\"";
	if (hasGroup())
		rOut << " gid=\"" << getAttr() << "\"";
	if (hasConfig())
		rOut << " config=\"" << getConfig() << "\"";
	rOut << ">";
	rOut << getPath() << "</file>";
}

// attribute structure for XMLFiles
structValidAttrs g_paFilesAttrs[] =
{
	{0x0000,    false, false, "attr", XATTRTYPE_INTEGER, {NULL}},
	{0x0001,    false, false, "gid",  XATTRTYPE_STRING,  {"*", NULL}},
	{0x0002,    false, false, "uid",  XATTRTYPE_STRING,  {"*", NULL}},
	{XATTR_END, false, false, "end",  XATTRTYPE_NONE,    {NULL}}
};

bool XMLFiles::parseCreate(XMLAttrs* pAttrs,
						   XMLSpec* pSpec)
{
	// validate the attributes
	if (!pSpec || !pAttrs->validate(g_paFilesAttrs, (XMLBase*)pSpec))
		return false;

	pSpec->lastPackage().getFiles().setDefAttr(pAttrs->asString("attr"));
	pSpec->lastPackage().getFiles().setDefOwner(pAttrs->asString("uid"));
	pSpec->lastPackage().getFiles().setDefGroup(pAttrs->asString("gid"));
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
	m_sAttr.assign(rFiles.m_sAttr);
	m_sOwner.assign(rFiles.m_sOwner);
	m_sGroup.assign(rFiles.m_sGroup);
	m_vFiles = rFiles.m_vFiles;
}

XMLFiles::~XMLFiles()
{
}

XMLFiles XMLFiles::operator=(XMLFiles files)
{
	m_sAttr.assign(files.m_sAttr);
	m_sOwner.assign(files.m_sOwner);
	m_sGroup.assign(files.m_sGroup);
	m_vFiles = files.m_vFiles;
	return *this;
}

void XMLFiles::toSpecFile(ostream& rOut)
{
	if (numFiles()) {
		if (hasDefAttr() || hasDefOwner() || hasDefGroup()) {
			rOut << "%defattr(";
			rOut << (hasDefAttr() ? getDefAttr() : "-");
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
		if (hasDefAttr())
			rOut << " attr=\"" << getDefAttr() << "\"";
		if (hasDefOwner())
			rOut << " uid=\"" << getDefOwner() << "\"";
		if (hasDefGroup())
			rOut << " gid=\"" << getDefGroup() << "\"";
		rOut << ">";
		for (unsigned int i = 0; i < numFiles(); i++)
			getFile(i).toXMLFile(rOut);
		rOut << endl << "\t\t</files>";
	}
}
