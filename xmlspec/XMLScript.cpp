// standard includes
#include <stdio.h>

// our includes
#include "XMLPackage.h"
#include "XMLScript.h"
#include "XMLSpec.h"

// attribute structure for XMLScript
structValidAttrs g_paScriptAttrs[] =
{
	{0x0000,    false, false, "dir",         XATTRTYPE_STRING, {"*", NULL}},
	{0x0001,    false, false, "interpreter", XATTRTYPE_STRING, {"*", NULL}},
	{XATTR_END, false, false, "end",         XATTRTYPE_NONE,   {NULL}}
};

bool XMLScript::parseCreate(XMLAttrs* pAttrs,
							const char* szScript,
							XMLScripts& rContainer)
{
	if (!pAttrs->validate(g_paScriptAttrs, (XMLBase*)pAttrs))
		return false;
	XMLScript script(szScript,
					 pAttrs->asString("interpreter"),
					 pAttrs->asString("dir"));
	rContainer.add(script);
	return true;
}

XMLScript::XMLScript(const char* szScript,
					 const char* szInterpreter,
					 const char* szDir)
	: XMLBase()
{
	if (szScript)
		m_sValue.assign(szScript);
	if (szInterpreter)
		m_sInterpreter.assign(szInterpreter);
	if (szDir)
		m_sDir.assign(szDir);
}

XMLScript::XMLScript(const XMLScript& rScript)
	: XMLBase()
{
	m_sValue.assign(rScript.m_sValue);
	m_sInterpreter.assign(rScript.m_sInterpreter);
	m_sDir.assign(rScript.m_sDir);
}

XMLScript::~XMLScript()
{
}

XMLScript XMLScript::operator=(XMLScript script)
{
	m_sValue.assign(script.m_sValue);
	m_sInterpreter.assign(script.m_sInterpreter);
	m_sDir.assign(script.m_sDir);
}

void XMLScript::toSpecFile(ostream& rOut)
{
	if (hasDirectory())
		rOut << "cd " << getDirectory() << endl;
	rOut << getValue() << endl;
}

void XMLScript::toXMLFile(ostream& rOut,
						  const char* szIndent)
{
	rOut << endl << szIndent << "\t\t<script";
	if (hasDirectory())
		rOut << " dir=\"" << getDirectory() << "\"";
	rOut << ">" << getValue() << "</script>";
}

void XMLScript::toRPMStruct(StringBuf* pSB)
{
	if (hasDirectory()) {
		char szBuff[getDirectoryLen()+3+1]; // 3 == strlen("cd ")
		sprintf(szBuff, "cd %s", getDirectory());
		appendStringBuf(*pSB, szBuff);
	}
	appendStringBuf(*pSB, getValue());
}

bool XMLScripts::addPrepScript(XMLAttrs* pAttrs,
							   const char* szScript,
							   XMLSpec* pSpec)
{
	// no spec or already set
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->getPrep());
}

bool XMLScripts::addBuildScript(XMLAttrs* pAttrs,
								const char* szScript,
								XMLSpec* pSpec)
{
	// no spec or already set
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->getBuild());
}

bool XMLScripts::addInstallScript(XMLAttrs* pAttrs,
								  const char* szScript,
								  XMLSpec* pSpec)
{
	// no spec or already set
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->getInstall());
}

bool XMLScripts::addCleanScript(XMLAttrs* pAttrs,
								const char* szScript,
								XMLSpec* pSpec)
{
	// no spec or already set
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->getClean());
}

XMLScripts::XMLScripts()
	: XMLBase()
{
}

XMLScripts::XMLScripts(const XMLScripts& rContainer)
	: XMLBase()
{
	m_vScripts = rContainer.m_vScripts;
}

XMLScripts::~XMLScripts()
{
}

void XMLScripts::toSpecFile(ostream& rOut,
							const char* szTag)
{
	if (numScripts()) {
		rOut << endl << "%" << szTag << endl;
		for (unsigned int i = 0; i < numScripts(); i++)
			getScript(i).toSpecFile(rOut);
	}
}

void XMLScripts::toXMLFile(ostream& rOut,
						   const char* szTag)
{
	if (numScripts()) {
		rOut << endl << "\t<" << szTag << ">";
		for (unsigned int i = 0; i < numScripts(); i++)
			getScript(i).toXMLFile(rOut, "");
		rOut << endl << "\t</" << szTag << ">";
	}
}

bool XMLPackageScripts::addPreScript(XMLAttrs* pAttrs,
									 const char* szScript,
									 XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->lastPackage().getPre());
}

bool XMLPackageScripts::addPostScript(XMLAttrs* pAttrs,
									  const char* szScript,
									  XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->lastPackage().getPost());
}

bool XMLPackageScripts::addPreUnScript(XMLAttrs* pAttrs,
									   const char* szScript,
									   XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->lastPackage().getPreUn());
}

bool XMLPackageScripts::addPostUnScript(XMLAttrs* pAttrs,
										const char* szScript,
										XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->lastPackage().getPostUn());
}

bool XMLPackageScripts::addVerifyScript(XMLAttrs* pAttrs,
										const char* szScript,
										XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	return XMLScript::parseCreate(pAttrs, szScript, pSpec->lastPackage().getVerify());
}

bool XMLPackageScripts::createPreScripts(XMLAttrs* pAttrs,
										 XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	pSpec->lastPackage().getPre().setInterpreter(pAttrs->asString("interpreter"));
	return true;
}

bool XMLPackageScripts::createPostScripts(XMLAttrs* pAttrs,
										  XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	pSpec->lastPackage().getPost().setInterpreter(pAttrs->asString("interpreter"));
	return true;
}

bool XMLPackageScripts::createPreUnScripts(XMLAttrs* pAttrs,
										   XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	pSpec->lastPackage().getPreUn().setInterpreter(pAttrs->asString("interpreter"));
	return true;
}

bool XMLPackageScripts::createPostUnScripts(XMLAttrs* pAttrs,
											XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	pSpec->lastPackage().getPostUn().setInterpreter(pAttrs->asString("interpreter"));
	return true;
}

bool XMLPackageScripts::createVerifyScripts(XMLAttrs* pAttrs,
											XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	pSpec->lastPackage().getVerify().setInterpreter(pAttrs->asString("interpreter"));
	return true;
}

XMLPackageScripts::XMLPackageScripts()
	: XMLScripts()
{
}

XMLPackageScripts::XMLPackageScripts(const XMLPackageScripts& rContainer)
	: XMLScripts(rContainer)
{
}

XMLPackageScripts::~XMLPackageScripts()
{
}

void XMLPackageScripts::toSpecFile(ostream& rOut,
								   const char* szTag)
{
	// NOTE: header not done here, but by "package"
	for (unsigned int i = 0; i < numScripts(); i++)
		getScript(i).toSpecFile(rOut);
}

void XMLPackageScripts::toXMLFile(ostream& rOut,
								  const char* szTag)
{
	if (numScripts()) {
		rOut << endl << "\t\t<" << szTag << ">";
		for (unsigned int i = 0; i < numScripts(); i++)
			getScript(i).toXMLFile(rOut, "\t");
		rOut << endl << "\t\t</" << szTag << ">";
	}
}
