#ifndef _H_XMLFILES_
#define _H_XMLFILES_

// standard C++ includes
#include <iostream>
#include <string>
#include <vector>

// standard includes
#include <stdio.h>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"

// rpm includes
#include <rpmbuild.h>

// forward class definitions
class XMLPackage;
class XMLFiles;
class XMLSpec;

using namespace std;

// <file ...>
class XMLFile : public XMLBase
{
//
// object creation static functions
//
public:
	/**
	 * Creates a file object and add it to the correct XMLFiles
	 * container.
	 * .
	 * @param pAttrs The attributes in the XML tag
	 * @param szPath The file path
	 * @param pSpec The spec to which these files belong
	 * @return true on success, false otherwise
	 **/
	static bool parseCreate(XMLAttrs* pAttrs,
							const char* szPath,
							XMLSpec* pSpec);

//
// constructors/destructor
//
public:
	/**
	 * Default contructor
	 * .
	 * @param szMode The file's mode (NULL if default)
	 * @param szOwner The file's owner (NULL if default)
	 * @param szGroup The file's group (NULL if default)
	 * @param szConfig The configuration parameter
	 * @param szPath The file path
	 * @return none
	 **/
	XMLFile(const char* szMode,
			const char* szUser,
			const char* szGroup,
			const char* szConfig,
			const char* szPath);

	/**
	 * Copy constructire
	 * .
	 * @param rFile Reference to the object to copy
	 * @return none
	 **/
	 XMLFile(const XMLFile& rFile);

	/**
	 * Default destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLFile();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param file The file that we wish to copy
	 * @return a copy of the original
	 **/
	XMLFile operator=(XMLFile file);

//
// member functions
//
public:
	/**
	 * Outputs the obkect to an RPM spec file
	 * .
	 * @param rOut Reference to the output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Outputs the object to an XML spec file
	 * .
	 * @param rOut Reference to the output stream
	 * @return none
	 **/
	 void toXMLFile(ostream& rOut);

//
// member variable get/set functions
//
public:
	/**
	 * Returns the file path
	 * .
	 * @param none
	 * @return string containing the path
	 **/
	const char* getPath()
	{
		return m_sPath.c_str();
	}

	/**
	 * Sets the file path
	 * .
	 * @param szPath The path to set
	 * @return none
	 **/
	void setPath(const char* szPath)
	{
		if (szPath)
			m_sPath.assign(szPath);
	}

	/**
	 * Checks for a file mode
	 * .
	 * @param none
	 * @return true if we have one, false otherwise
	 **/
	bool hasMode()
	{
		return m_sMode.length() ? true : false;
	}

	/**
	 * Returns the file mode
	 * .
	 * @param none
	 * @return the mode string
	 **/
	const char* getMode()
	{
		return m_sMode.c_str();
	}

	/**
	 * Sets the file mode
	 * .
	 * @param szMode The mode to set
	 * @return none
	 **/
	void setMode(const char* szMode)
	{
		if (szMode)
			m_sMode.assign(szMode);
	}

	/**
	 * Checks if we have a file owner
	 * .
	 * @param none
	 * @return true if we have an owner, false otherwise
	 **/
	bool hasOwner()
	{
		return m_sOwner.length() ? true : false;
	}

	/**
	 * Returns the file owner
	 * .
	 * @param none
	 * @return the owner as a string
	 **/
	const char* getOwner()
	{
		return m_sOwner.c_str();
	}

	/**
	 * Sets the file owner
	 * .
	 * @param szOwner The file owner
	 * @return none
	 **/
	void setOwner(const char* szOwner)
	{
		if (szOwner)
			m_sOwner.assign(szOwner);
	}

	/**
	 * Checks for a file group
	 * .
	 * @param none
	 * @return true if we have a group, false otherwise
	 **/
	bool hasGroup()
	{
		return m_sGroup.length() ? true : false;
	}

	/**
	 * Returns the file group
	 * .
	 * @param none
	 * @return string containing the group
	 **/
	const char* getGroup()
	{
		return m_sGroup.c_str();
	}

	/**
	 * Sets the file group
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
	 * Checks for config directives
	 * .
	 * @param none
	 * @return true if we have one, false otherwise
	 **/
	bool hasConfig()
	{
		return m_sConfig.length() ? true : false;
	}

	/**
	 * Returns the config attribute
	 * .
	 * @param none
	 * @return the sttribute string
	 **/
	const char* getConfig()
	{
		return m_sConfig.c_str();
	}

	/**
	 * Sets the config attribute
	 * .
	 * @param szConfig The configuration
	 * @return none
	 **/
	void setConfig(const char* szConfig)
	{
		if (szConfig)
			m_sConfig.assign(szConfig);
	}

//
// member variables
//
protected:
	string m_sPath;
	string m_sMode;
	string m_sOwner;
	string m_sGroup;
	string m_sConfig;
};

// <files ...>
class XMLFiles : public XMLBase
{
//
// object creation static functions
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
	 * Creates file objects from an RPM Spec structure
	 * .
	 * @param pPackage Pointer to the package
	 * @param pSpec Pointer to the RPM spec
	 * @param pXSpec pointer to the XMLSpec object to populate
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
	XMLFiles();

	/**
	 * Copy constructor
	 * .
	 * @param rFiles Reference to the object to copy
	 * @return none
	 **/
	XMLFiles(const XMLFiles& rFiles);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLFiles();

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
	 * @param rOut Output stream
	 * @return none
	 **/
	void toXMLFile(ostream& rOut);

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param files XMLFiles object to copy
	 * @return copied object
	 **/
	XMLFiles operator=(XMLFiles files);

//
// member variable get/set functions
//
public:
	/**
	 * Adds a file to our file list
	 * .
	 * @param rFile File to add
	 * @return none
	 **/
	void addFile(XMLFile& rFile)
	{
		m_vFiles.push_back(rFile);
	}

	/**
	 * Returns the number of files in our list
	 * .
	 * @param none
	 * @return none
	 **/
	unsigned int numFiles()
	{
		return m_vFiles.size();
	}

	/**
	 * Returns a specific file
	 * .
	 * @param nNum Number of the file to return
	 * @return the file object
	 **/
	XMLFile& getFile(unsigned int nNum)
	{
		return m_vFiles[nNum];
	}

	/**
	 * Checks for a default mode
	 * .
	 * @param none
	 * @return true if we have a default mode, false otherwise
	 **/
	bool hasDefMode()
	{
		return m_sMode.length() ? true : false;
	}

	/**
	 * Sets the default mode
	 * .
	 * @param szMode The mode value
	 * @return none
	 **/
	void setDefMode(const char* szMode)
	{
		if (szMode)
			m_sMode.assign(szMode);
	}

	/**
	 * Returns the default mode
	 * .
	 * @param none
	 * @return string containing the mode
	 **/
	const char* getDefMode()
	{
		return m_sMode.c_str();
	}

	/**
	 * Check if we have a default owner
	 * .
	 * @param none
	 * @return true if we have an owner, false otherwise
	 **/
	bool hasDefOwner()
	{
		return m_sOwner.length() ? true : false;
	}

	/**
	 * Sets the default owner
	 * .
	 * @param szOwner The owner
	 * @return none
	 **/
	void setDefOwner(const char* szOwner)
	{
		if (szOwner)
			m_sOwner.assign(szOwner);
	}

	/**
	 * Returns the default owner
	 * .
	 * @param none
	 * @return the owner string
	 **/
	const char* getDefOwner()
	{
		return m_sOwner.c_str();
	}

	/**
	 * Checks if we have a default group
	 * .
	 * @param none
	 * @return true if we have an owner, false otherwise
	 **/
	bool hasDefGroup()
	{
		return m_sGroup.length() ? true : false;
	}

	/**
	 * Sets the default group
	 * .
	 * @param szGroup The group to set
	 * @return none
	 **/
	void setDefGroup(const char* szGroup)
	{
		if (szGroup)
			m_sGroup.assign(szGroup);
	}

	/**
	 * Gets the default group
	 * .
	 * @param none
	 * @return string representation of the group
	 **/
	const char* getDefGroup()
	{
		return m_sGroup.c_str();
	}

//
// member variables
//
protected:
	string          m_sMode;
	string          m_sOwner;
	string          m_sGroup;
	vector<XMLFile> m_vFiles;
};

#endif
