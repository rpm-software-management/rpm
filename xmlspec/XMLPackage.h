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
#include "XMLText.h"

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
	 * Creates package objects from an RPM Spec structure
	 * .
	 * @param pPackage Pointer to the start package
	 * @param pSpec pointer to the RPM spec
	 * @param pXSpec Pointer to the spec object to populate
	 * @return true on success, false otherwise
	 **/
	static bool structCreate(PackageStruct* pPackage,
							 Spec pSpec,
							 XMLSpec* pXSpec);

	/**
	 * Adds a description for the last package
	 * .
	 * @param pAttrs The attributes
	 * @param szDescription the description
	 * @param pSpec The spec containing the package
	 * @return true on success, false otherwise
	 **/
	static bool addDescription(XMLAttrs* pAttrs,
							   const char* szDescription,
							   XMLSpec* pSpec);

	/**
	 * Adds the summary for the last added package
	 * .
	 * @param pAttrs The attributes
	 * @param szSummary The summary to set
	 * @param pSpec The spec contraining the package
	 * @return trus on success, false otherwise
	 **/
	static bool addSummary(XMLAttrs* pAttrs,
						   const char* szSummary,
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
	 * @param bAutoReq Auto Requires
	 * @param bAutoProv Auto Provides
	 * @param bSub true if this is a sub-package
	 * @return none
	 **/
	XMLPackage(const char* szName,
			   const char* szGroup,
			   bool bAutoReq,
			   bool bAutoProv,
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
	 * Sets the package name
	 * .
	 * @param szName The name to set
	 * @return none
	 **/
	void setName(const char* szName)
	{
		if (szName)
			m_sName.assign(szName);
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
	 * Sets the group
	 * .
	 * @param szGroup The group to set
	 * @return none
	 **/
	void setGroup(const char* szGroup)
	{
		if (szGroup)
			m_sGroup.assign(szGroup);
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
	 * Sets the sub-package status
	 * .
	 * @param bSub The sub-package status
	 * @return none
	 **/
	void setSubPackage(bool bSub)
	{
		m_bSub = bSub;
	}

	/**
	 * Tests for auto requires
	 * .
	 * @param none
	 * @return true if we have auto requires, false otherwise
	 **/
	bool hasAutoRequires()
	{
		return m_bAutoReq;
	}

	/**
	 * Sets the auto requires state
	 * .
	 * @param bAutoReq The auto requires state
	 * @return none
	 **/
	void setAutoRequires(bool bAutoReq)
	{
		m_bAutoReq = bAutoReq;
	}

	/**
	 * Tests for auto requires
	 * .
	 * @param none
	 * @return true if we have auto requires, false otherwise
	 **/
	bool hasAutoProvides()
	{
		return m_bAutoProv;
	}

	/**
	 * Sets the auto provides status
	 * .
	 * @param bAutoProv The auto provides status
	 * @return none
	 **/
	void setAutoProvides(bool bAutoProv)
	{
		m_bAutoProv = bAutoProv;
	}

	/**
	 * Gets the number of summaries
	 * .
	 * @param none
	 * @return the number of summaries
	 **/
	unsigned int numSummaries()
	{
		return m_vSummaries.size();
	}

	/**
	 * Gets a summary
	 * .
	 * @param nNum The number to get
	 * @return the summary
	 **/
	XMLText& getSummary(unsigned int nNum)
	{
		return m_vSummaries[nNum];
	}

	/**
	 * Adds a summary
	 * .
	 * @param szSummary the summary
	 * @param szLang the language
	 * @return none
	 **/
	void addSummary(const char* szSummary,
					const char* szLang = NULL)
	{
		if (szSummary && strlen(szSummary)) {
			XMLText summary(szSummary, szLang);
			m_vSummaries.push_back(summary);
		}
	}

	/**
	 * Gets the number of descriptions
	 * .
	 * @param none
	 * @return the number in our list
	 **/
	unsigned int numDescriptions()
	{
		return m_vDescriptions.size();
	}

	/**
	 * Get a description
	 * .
	 * @param nNum The description to get
	 * @return Reference to XMLText object
	 **/
	XMLText& getDescription(unsigned int nNum)
	{
		return m_vDescriptions[nNum];
	}

	/**
	 * Adds a description
	 * .
	 * @param szDescription The description string
	 * @param szLang The language
	 * @return nonew
	 **/
	void addDescription(const char* szDescription,
						const char* szLang = NULL)
	{
		if (szDescription && strlen(szDescription)) {
			XMLText description(szDescription, szLang);
			m_vDescriptions.push_back(description);
		}
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
	bool                m_bSub;
	bool                m_bAutoReq;
	bool                m_bAutoProv;
	vector<XMLText>     m_vSummaries;
	vector<XMLText>     m_vDescriptions;
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
