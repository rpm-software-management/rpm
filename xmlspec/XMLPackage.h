#ifndef _H_XMLPACKAGE_
#define _H_XMLPACKAGE_

// standard C++ includes
#include <iostream>
#include <string>
#include <vector>

// standard includes
#include <stdio.h>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"
#include "XMLFiles.h"
#include "XMLRequires.h"
#include "XMLScript.h"

// rpm includes
#include <rpmbuild.h>

// forward class declarations
class XMLSpec;

using namespace std;

class XMLPackage : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Creates an object as parsed from an XML spec
	 * .
	 * @param pAttrs XML atrtributes to use
	 * @param pSpec The spec to which we are adding this object to
	 * @return true on success, false otherwise
	 **/
	static bool parseCreate(XMLAttrs* pAttrs,
							XMLSpec* pSpec);

	/**
	 * Sets the description for the last package
	 * .
	 * @param szDescription the description
	 * @param pSpec The spec containing the package
	 * @return true on success, false otherwise
	 **/
	static bool setDescription(const char* szDescription,
							   XMLSpec* pSpec);

	/**
	 * Sets the summary for the last added package
	 * .
	 * @param szSummary The summary to set
	 * @param pSpec The spec contraining the package
	 * @return trus on success, false otherwise
	 **/
	static bool setSummary(const char* szSummary,
						   XMLSpec* pSpec);

//
// constructors/destructors
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szName The package name
	 * @param szGroup The group this package belongs to
	 * @param bSub true if this is a sub-package
	 * @return none
	 **/
	XMLPackage(const char* szName,
			   const char* szGroup,
			   bool bSub);

	/**
	 * Copy constructor
	 * .
	 * @param rPackage The package to copy from
	 * @return none
	 **/
	XMLPackage(const XMLPackage& rPackage);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLPackage();

//
// public member functions
//
public:
	/**
	 * Converts the object to a spec file
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Converts the scripts part of the object to a spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toScriptsSpecFile(ostream& rOut);

	/**
	 * Converts the files part of the object to a spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toFilesSpecFile(ostream& rOut);

	/**
	 * Converts the object to an XML spec
	 * .
	 * @param rOut The output stream
	 * @return none
	 **/
	void toXMLFile(ostream& rOut);

	/**
	 * Converts the object to an RPM structure
	 * .
	 * @param Spec The main Spec object
	 * @param none
	 **/
	void toRPMStruct(Spec spec);

//
// member variable get/set functions
//
public:
	/**
	 * Checks of we have a package name
	 * .
	 * @param none
	 * @return true if we have a name, false otherwise
	 **/
	bool hasName()
	{
		return m_sName.length() ? true : false;
	}

	/**
	 * Gets the package name
	 * .
	 * @param none
	 * @return string containing the package name
	 **/
	const char* getName()
	{
		return m_sName.c_str();
	}

	/**
	 * Checks if we have a group
	 * .
	 * @param none
	 * @return true if we have a group, false otherwise
	 **/
	bool hasGroup()
	{
		return m_sGroup.length() ? true : false;
	}

	/**
	 * Returns the group
	 * .
	 * @param none
	 * @return string containing the group
	 **/
	const char* getGroup()
	{
		return m_sGroup.c_str();
	}

	/**
	 * Tests if we are a sub-package
	 * .
	 * @param none
	 * @return true if this is a sub-package
	 **/
	bool isSubPackage()
	{
		return m_bSub;
	}

	/**
	 * Checks if we have a summary
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasSummary()
	{
		return m_sSummary.length() ? true : false;
	}

	/**
	 * Gets the summary
	 * .
	 * @param none
	 * @return the summary string
	 **/
	const char* getSummary()
	{
		return m_sSummary.c_str();
	}

	/**
	 * Set the summary
	 * .
	 * @param szSummary the summary
	 * @return none
	 **/
	void setSummary(const char* szSummary)
	{
		if (szSummary)
			m_sSummary.assign(szSummary);
	}

	/**
	 * Checks if we have a description
	 * .
	 * @param none
	 * @return true if available, false otherwise
	 **/
	bool hasDescription()
	{
		return m_sDescription.length() ? true : false;
	}

	/**
	 * Get the description
	 * .
	 * @param none
	 * @return string containing the description
	 **/
	const char* getDescription()
	{
		return m_sDescription.c_str();
	}

	/**
	 * Sets the description
	 * .
	 * @param szDescription The description string
	 * @return nonew
	 **/
	void setDescription(const char* szDescription)
	{
		if (szDescription)
			m_sDescription.assign(szDescription);
	}

	/**
	 * Gets the provides
	 * .
	 * @param none
	 * @return reference to the provides
	 **/
	XMLPackageContainer& getProvides()
	{
		return m_Provides;
	}

	/**
	 * Gets the package requires
	 * .
	 * @param none
	 * @return reference to the requires
	 **/
	XMLPackageContainer& getRequires()
	{
		return m_Requires;
	}

	/**
	 * Get the buildrequires
	 * .
	 * @param none
	 * @return reference to the buildrequires
	 **/
	XMLPackageContainer& getBuildRequires()
	{
		return m_BuildRequires;
	}

	/**
	 * Gets the obsoletes
	 * .
	 * @param none
	 * @return reference to the obsoletes
	 **/
	XMLPackageContainer& getObsoletes()
	{
		return m_Obsoletes;
	}

	/**
	 * Gets the files
	 * .
	 * @param none
	 * @return reference to the files
	 **/
	XMLFiles& getFiles()
	{
		return m_Files;
	}

	/**
	 * Gets the pre section
	 * .
	 * @param none
	 * @return reference to the pre section
	 **/
	XMLPackageScripts& getPre()
	{
		return m_Pre;
	}

	/**
	 * Gets the post section
	 * .
	 * @param none
	 * @return reference to the post section
	 **/
	XMLPackageScripts& getPost()
	{
		return m_Post;
	}

	/**
	 * Gets the preun section
	 * .
	 * @param none
	 * @return reference to the preun section
	 **/
	XMLPackageScripts& getPreUn()
	{
		return m_PreUn;
	}

	/**
	 * Gets the postun section
	 * .
	 * @param none
	 * @return reference to the postun section
	 **/
	XMLPackageScripts& getPostUn()
	{
		return m_PostUn;
	}

	/**
	 * Gets the verify section
	 * .
	 * @param none
	 * @return reference to the verify section
	 **/
	XMLPackageScripts& getVerify()
	{
		return m_Verify;
	}

//
// member variables
//
protected:
	string              m_sName;
	string              m_sGroup;
	string              m_sSummary;
	string              m_sDescription;
	bool                m_bSub;
	XMLPackageContainer m_Requires;
	XMLPackageContainer m_BuildRequires;
	XMLPackageContainer m_Provides;
	XMLPackageContainer m_Obsoletes;
	XMLPackageScripts   m_Pre;
	XMLPackageScripts   m_PreUn;
	XMLPackageScripts   m_Post;
	XMLPackageScripts   m_PostUn;
	XMLPackageScripts   m_Verify;
	XMLFiles            m_Files;
};

#endif
