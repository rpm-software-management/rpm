#ifndef _H_XMLREQUIRES_
#define _H_XMLREQUIRES_

// standard C++ includes
#include <iostream>
#include <string>
#include <vector>

// standard C includes
#include <stdio.h>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"

// forward definitions
class XMLPackage;
class XMLPackageContainer;
class XMLSpec;

using namespace std;

//<package ...> (after requires, buildrequires, obsoletes, provides)
class XMLPackageEntry : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Creates an object as parsed from an XML spec
	 * .
	 * @param pAttrs XML atrtributes to use
	 * @param rContainer The container to which to add the object
	 * @return true on success, false otherwise
	 **/
	static bool parseCreate(XMLAttrs* pAttrs,
							XMLPackageContainer& rContainer);

//
// constructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szName Name of the package
	 * @param szVersion version of the package
	 * @param szCmp the comparator (eq,lt,le,gt,ge)
	 * @return none
	 **/
	XMLPackageEntry(const char* szName,
					const char* szVersion,
					const char* szCmp);

	/**
	 * Copy constructor
	 * .
	 * @param rEntry reference to the entrry to copy
	 * @return none
	 **/
	XMLPackageEntry(const XMLPackageEntry& rEntry);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLPackageEntry();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param entry The entry to assigne to
	 * @return thye modified object
	 **/
	XMLPackageEntry operator=(XMLPackageEntry entry);
//
// member functions
//
public:
	/**
	 * Converts the object into an RPM spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut output stream
	 * @return none
	 **/
	void toXMLFile(ostream& rOut);

//
// member variable get/set functions
//
public:
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
	 * Gets the comparision string
	 * .
	 * @param none
	 * @return string with the comparator (=, <, <=, >, >=, <>)
	 **/
	 const char* getCompare()
	 {
	 	return m_sCmp.c_str();
	 }

//
// member variables
//
protected:
	string m_sName;
	string m_sVersion;
	string m_sCmp;
};

//<requires,obsoletes,buildrequires,provides ...>
class XMLPackageContainer : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Adds a require
	 * .
	 * @param pAttrs XML attributes
	 * @param pSpecx pointer to the spec to add to
	 * @return true on success, false otherwise
	 **/
	static bool addRequire(XMLAttrs* pAttrs,
						   XMLSpec* pSpec);

	/**
	 * Adds a buildrequire
	 * .
	 * @param pAttrs XML attributes
	 * @param pSpecx pointer to the spec to add to
	 * @return true on success, false otherwise
	 **/
	static bool addBuildRequire(XMLAttrs* pAttrs,
								XMLSpec* pSpec);

	/**
	 * Adds a provide
	 * .
	 * @param pAttrs XML attributes
	 * @param pSpecx pointer to the spec to add to
	 * @return true on success, false otherwise
	 **/
	static bool addProvide(XMLAttrs* pAttrs,
						   XMLSpec* pSpec);

	/**
	 * Adds an obsolete
	 * .
	 * @param pAttrs XML attributes
	 * @param pSpecx pointer to the spec to add to
	 * @return true on success, false otherwise
	 **/
	static bool addObsolete(XMLAttrs* pAttrs,
							XMLSpec* pSpec);

	/**
	 * Adds requires/provides/obsoletes from RPM structures
	 * .
	 * @param pPackage pointer to the RPM package
	 * @param pSpec pointer to the RPM spec
	 * @param pXSpec pointer to the XML spec to populate
	 * @return true on success, false otherwise
	 **/
	static bool structCreate(PackageStruct* pPackage,
							 Spec pSpec,
							 XMLSpec* pXSpec);

//
// constructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param none
	 * @return none
	 **/
	XMLPackageContainer();

	/**
	 * Copy constructor
	 * .
	 * @param rContainer The container to copy
	 * @return none
	 **/
	 XMLPackageContainer(const XMLPackageContainer& rContainer);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	virtual ~XMLPackageContainer();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param container The container to copy
	 * @return a copy of the object
	 **/
	XMLPackageContainer operator=(XMLPackageContainer container);

//
// member functions
//
public:
	/**
	 * Converts the object into an RPM spec
	 * .
	 * @param rOut Output stream
	 * @param szTag the tag for this object (eg. buildrequires)
	 * @return none
	 **/
	virtual void toSpecFile(ostream& rOut,
							const char* szTag);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @para szTag the tag for this object (eg. buildrequires)
	 * @return none
	 **/
	virtual void toXMLFile(ostream& rOut,
						   const char* szTag);

//
// member variable get/set functions
//
public:
	/**
	 * Gets the number of entries
	 * .
	 * @param none
	 * @return the number of entries
	 **/
	unsigned int numEntries()
	{
		return m_vPackages.size();
	}

	/**
	 * Gets a specific entry
	 * .
	 * @param nNum The number of the entry
	 * @return reference to the entry
	 **/
	XMLPackageEntry& getEntry(unsigned int nNum)
	{
		return m_vPackages[nNum];
	}

	/**
	 * Adds an entry
	 * .
	 * @param rPackage the entry to add
	 * @return none
	 **/
	void addEntry(XMLPackageEntry& rPackage)
	{
		m_vPackages.push_back(rPackage);
	}

//
// member variables
//
protected:
	vector<XMLPackageEntry> m_vPackages;
};

#endif
