#ifndef _H_XMLSPEC_
#define _H_XMLSPEC_

// standard C++ includes
#include <iostream>
#include <string>
#include <vector>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"
#include "XMLChangelog.h"
#include "XMLMacro.h"
#include "XMLPackage.h"
#include "XMLScript.h"
#include "XMLSource.h"

// rpm includes
#include <rpmbuild.h>

using namespace std;

// <spec ...>
class XMLSpec : public XMLBase
{
//
// static object creation functions
//
public:
	/**
	 * Creates an XMLSpec from values parsed
	 * .
	 * @param pAttrs The XML attributes
	 * @param szFilename The XML spec filename
	 * @return Pointer to the created spec
	 **/
	static XMLSpec* parseCreate(XMLAttrs* pAttrs,
								const char* szFilename);

	/**
	 * Creates and XMLSpec from an RPM Spec structure
	 * .
	 * @param pSpec The RPM spec structure
	 * @return Pointer to the created spec
	 **/
	static XMLSpec* structCreate(Spec pSpec);

//
// constructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szFilename Filename of the spec on disk
	 * @param szName spec name
	 * @param szVersion Spec version
	 * @param szRelease spec release
	 * @param szEpoch spec epoch
	 * @param szDistribution spec distribution
	 * @param szVendor spec vendor
	 * @param szPackage spec packager
	 * @param szPkgEmail email address for the packager
	 * @param szCopyright spec copyright/licence
	 * @param szURL main package url
	 * @param szBuildRoot buildroot
	 * @return none
	 **/
	XMLSpec(const char* szFilename,
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
			const char* szBuildRoot);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLSpec();

//
// public member functions
//
public:
	/**
	 * Converts the spec object to a normal RPM spec file
	 * .
	 * @param rOut Reference to the stream to write the information to
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Converts the spec object to an XML spec file
	 * .
	 * @param rOut Reference to the stream to write the information to
	 * @return none
	 **/
	 void toXMLFile(ostream& rOut);

	 /**
	  * Converts the spec object to an internal RPM structure
	  * .
	  * @param none
	  * @return the created RPM structure
	  **/
	  void toRPMStruct(Spec* pRPMSpec);

//
// member variable get/set functions
//
public:
	/**
	 * Adds a package to the internal list
	 * .
	 * @param rPkg Reference to the package to add
	 * @return none
	 **/
	void addPackage(XMLPackage& rPkg)
	{
		m_vPackages.push_back(rPkg);
	}

	/**
	 * Gets the number of packages
	 * .
	 * @param none
	 * @return the number of packages
	 **/
	unsigned int numPackages()
	{
		return m_vPackages.size();
	}

	/**
	 * Gets a specific package
	 * .
	 * @param nNum The package number
	 * @return the required package
	 **/
	XMLPackage& getPackage(unsigned int nNum)
	{
		return m_vPackages[nNum];
	}

	/**
	 * Gets the last package added
	 * .
	 * @param none
	 * @return the last package added
	 **/
	XMLPackage& lastPackage()
	{
		return m_vPackages[numPackages()-1];
	}

	/**
	 * Adds a source to the internal list
	 * .
	 * @param rSource Reference to the source to add
	 * @return none
	 **/
	void addSource(XMLSource& rSource)
	{
		m_vSources.push_back(rSource);
	}

	/**
	 * Gets the number of sources
	 * .
	 * @param none
	 * @return the number of sources
	 **/
	unsigned int numSources()
	{
		return m_vSources.size();
	}

	/**
	 * Gets a specific source
	 * .
	 * @param nNum The source number
	 * @return the required source
	 **/
	XMLSource& getSource(unsigned int nNum)
	{
		return m_vSources[nNum];
	}

	/**
	 * Gets the last source added
	 * .
	 * @param none
	 * @return the last source added
	 **/
	XMLSource& lastSource()
	{
		return m_vSources[numSources()-1];
	}

	/**
	 * Adds a source to the internal list
	 * .
	 * @param rSource Reference to the source to add
	 * @return none
	 **/
	void addNoSource(XMLNoSource& rSource)
	{
		m_vNoSources.push_back(rSource);
	}

	/**
	 * Gets the number of nosources
	 * .
	 * @param none
	 * @return the number of nsources
	 **/
	unsigned int numNoSources()
	{
		return m_vNoSources.size();
	}

	/**
	 * Gets a specific nosource
	 * .
	 * @param nNum The nosource number
	 * @return the required nosource
	 **/
	XMLNoSource& getNoSource(unsigned int nNum)
	{
		return m_vNoSources[nNum];
	}

	/**
	 * Gets the last nosource added
	 * .
	 * @param none
	 * @return the last nosource added
	 **/
	XMLNoSource& lastNoSource()
	{
		return m_vNoSources[numNoSources()-1];
	}

	/**
	 * Adds a patch to the internal list
	 * .
	 * @param rSource Reference to the patch to add
	 * @return none
	 **/
	void addPatch(XMLPatch& rSource)
	{
		m_vPatches.push_back(rSource);
	}

	/**
	 * Gets the number of patches
	 * .
	 * @param none
	 * @return the number of patches
	 **/
	unsigned int numPatches()
	{
		return m_vPatches.size();
	}

	/**
	 * Gets a specific patch
	 * .
	 * @param nNum The patch number
	 * @return the required patch
	 **/
	XMLPatch& getPatch(unsigned int nNum)
	{
		return m_vPatches[nNum];
	}

	/**
	 * Gets the last patch added
	 * .
	 * @param none
	 * @return the last patch added
	 **/
	XMLPatch& lastPatch()
	{
		return m_vPatches[numPatches()-1];
	}

	/**
	 * Adds a macro to the internal list
	 * .
	 * @param rMacro Reference to the macro to add
	 * @return none
	 **/
	void addXMacro(XMLMacro& rMacro)
	{
		m_vMacros.push_back(rMacro);
	}

	/**
	 * Gets the number of macros
	 * .
	 * @param none
	 * @return the number of macros
	 **/
	unsigned int numXMacros()
	{
		return m_vMacros.size();
	}

	/**
	 * Gets a specific macro
	 * .
	 * @param nNum The macro number
	 * @return the required macro
	 **/
	XMLMacro& getXMacro(unsigned int nNum)
	{
		return m_vMacros[nNum];
	}

	/**
	 * Checks if we have a filename
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasFilename()
	{
		return m_sFilename.length() ? true : false;
	}

	/**
	 * Gets the filename
	 * .
	 * @param none
	 * @return string containing the filename
	 **/
	const char* getFilename()
	{
		return m_sFilename.c_str();
	}

	/**
	 * Checks if we have a name
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasName()
	{
		return m_sName.length() ? true : false;
	}

	/**
	 * Gets the name
	 * .
	 * @param none
	 * @return string containing the name
	 **/
	const char* getName()
	{
		return m_sName.c_str();
	}

	/**
	 * Checks if we have a version
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasVersion()
	{
		return m_sVersion.length() ? true : false;
	}

	/**
	 * Gets the version
	 * .
	 * @param none
	 * @return string containing the version
	 **/
	const char* getVersion()
	{
		return m_sVersion.c_str();
	}

	/**
	 * Checks if we have a release
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasRelease()
	{
		return m_sRelease.length() ? true : false;
	}

	/**
	 * Gets the release
	 * .
	 * @param none
	 * @return string containing the release
	 **/
	const char* getRelease()
	{
		return m_sRelease.c_str();
	}

	/**
	 * Checks if we have a epoch
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasEpoch()
	{
		return m_sEpoch.length() ? true : false;
	}

	/**
	 * Gets the epoch
	 * .
	 * @param none
	 * @return string containing the epoch
	 **/
	const char* getEpoch()
	{
		return m_sEpoch.c_str();
	}

	/**
	 * Checks if we have a distribution
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasDistribution()
	{
		return m_sDistribution.length() ? true : false;
	}

	/**
	 * Gets the distribution
	 * .
	 * @param none
	 * @return string containing the distribution
	 **/
	const char* getDistribution()
	{
		return m_sDistribution.c_str();
	}

	/**
	 * Checks if we have a vendor
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasVendor()
	{
		return m_sVendor.length() ? true : false;
	}

	/**
	 * Gets the vendor
	 * .
	 * @param none
	 * @return string containing the vendor
	 **/
	const char* getVendor()
	{
		return m_sVendor.c_str();
	}

	/**
	 * Checks if we have a packager
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasPackager()
	{
		return m_sPackager.length() ? true : false;
	}

	/**
	 * Gets the packager
	 * .
	 * @param none
	 * @return string containing the packager
	 **/
	const char* getPackager()
	{
		return m_sPackager.c_str();
	}

	/**
	 * Checks if we have a packager email
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasPkgrEmail()
	{
		return m_sPkgrEmail.length() ? true : false;
	}

	/**
	 * Gets the packager's email address
	 * .
	 * @param none
	 * @return string containing the packager's email address
	 **/
	const char* getPkgrEmail()
	{
		return m_sPkgrEmail.c_str();
	}

	/**
	 * Checks if we have a copyright
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasCopyright()
	{
		return m_sCopyright.length() ? true : false;
	}

	/**
	 * Gets the copyright
	 * .
	 * @param none
	 * @return string containing the copyright
	 **/
	const char* getCopyright()
	{
		return m_sCopyright.c_str();
	}

	/**
	 * Checks if we have an URL
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasURL()
	{
		return m_sURL.length() ? true : false;
	}

	/**
	 * Gets the URL
	 * .
	 * @param none
	 * @return string containing the URL
	 **/
	const char* getURL()
	{
		return m_sURL.c_str();
	}

	/**
	 * Checks if we have a BuildRoot
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasBuildRoot()
	{
		return m_sBuildRoot.length() ? true : false;
	}

	/**
	 * Gets the buildroot
	 * .
	 * @param none
	 * @return string containing the buildroor
	 **/
	const char* getBuildRoot()
	{
		return m_sBuildRoot.c_str();
	}

	/**
	 * Gets the prep section
	 * .
	 * @param none
	 * @return reference to the prep section
	 **/
	XMLScripts& getPrep()
	{
		return m_Prep;
	}

	/**
	 * Gets the build section
	 * .
	 * @param none
	 * @return reference to the build section
	 **/
	XMLScripts& getBuild()
	{
		return m_Build;
	}

	/**
	 * Gets the install section
	 * .
	 * @param none
	 * @return reference to the install section
	 **/
	XMLScripts& getInstall()
	{
		return m_Install;
	}

	/**
	 * Gets the clean section
	 * .
	 * @param none
	 * @return reference to the clean section
	 **/
	XMLScripts& getClean()
	{
		return m_Clean;
	}

	/**
	 * Gets the changelog section
	 * .
	 * @param none
	 * @return reference to the changelog section
	 **/
	XMLChangelog& getChangelog()
	{
		return m_Changelog;
	}

//
// internal member variables
//
protected:
	string              m_sFilename;
	string              m_sName;
	string              m_sVersion;
	string              m_sRelease;
	string              m_sEpoch;
	string              m_sDistribution;
	string              m_sVendor;
	string              m_sPackager;
	string              m_sPkgrEmail;
	string              m_sCopyright;
	string              m_sURL;
	string              m_sBuildRoot;
	vector<XMLPackage>  m_vPackages;
	vector<XMLSource>   m_vSources;
	vector<XMLNoSource> m_vNoSources;
	vector<XMLPatch>    m_vPatches;
	vector<XMLMacro>    m_vMacros;
	XMLScripts          m_Prep;
	XMLScripts          m_Build;
	XMLScripts          m_Install;
	XMLScripts          m_Clean;
	XMLChangelog        m_Changelog;
};

#endif
