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
	 * @param szAttr The file's attribute (NULL if default)
	 * @param szOwner The file's owner (NULL if default)
	 * @param szGroup The file's group (NULL if default)
	 * @param szPath The file path
	 * @return none
	 **/
	XMLFile(const char* szAttr,
			const char* szOwner,
			const char* szGroup,
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
	 * Checks for a file attribute
	 * .
	 * @param none
	 * @return true if we have one, false otherwise
	 **/
	bool hasAttr()
	{
		return m_sAttr.length() ? true : false;
	}

	/**
	 * Returns the file attribute
	 * .
	 * @param none
	 * @return the sttribute string
	 **/
	const char* getAttr()
	{
		return m_sAttr.c_str();
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
	 * @param no0ne
	 * @return the owner as a string
	 **/
	const char* getOwner()
	{
		return m_sOwner.c_str();
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

//
// member variables
//
protected:
	string    m_sPath;
	string    m_sAttr;
	string    m_sOwner;
	string    m_sGroup;
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
	 * Checks for a default attribute
	 * .
	 * @param none
	 * @return true if we have a default attribute, false otherwise
	 **/
	bool hasDefAttr()
	{
		return m_sAttr.length() ? true : false;
	}

	/**
	 * Sets the default attribute
	 * .
	 * @param szAttr The attribute value
	 * @return none
	 **/
	void setDefAttr(const char* szAttr)
	{
		if (szAttr)
			m_sAttr.assign(szAttr);
	}

	/**
	 * Returns the default attribute
	 * .
	 * @param none
	 * @return string contating the attribute
	 **/
	const char* getDefAttr()
	{
		return m_sAttr.c_str();
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
	string          m_sAttr;
	string          m_sOwner;
	string          m_sGroup;
	vector<XMLFile> m_vFiles;
};

#endif
