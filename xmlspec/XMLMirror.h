#ifndef _H_XMLMIRROR_
#define _H_XMLMIRROR_

// standard C++ includes
#include <string>
#include <vector>
#include <iostream>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"

// forward declaration
class XMLSpec;

using namespace std;

// <mirror ...>
class XMLMirror : public XMLBase
{
//
// static object creation functions
//
public:
	/**
	 * static function for creation of an XMLMirror object
	 * .
	 * @param pAttrs Pointer to our attribute structure
	 * @param pSpec Pointer to our spec that is to ultimately
	 *              contain the object
	 * @param bPatch true if we are handling the mirror for a patch
	 * @return true on success, false otherwise
	 **/
	static bool parseCreate(XMLAttrs* pAttrs,
							XMLSpec* pSpec,
							bool bPatch = false);

//
// constructors/destructor
//
public:
	/**
	 * Default constructor for the XMLMirror object
	 * .
	 * @param szPath        Full path for the mirror
	 * @param szDescription Full mirror description
	 * @param szCoutry      Country code for the mirror
	 * @return none
	 **/
	XMLMirror(const char* szPath,
			  const char* szDescription,
			  const char* szCountry);

	/**
	 * Copy contructor
	 * .
	 * @param rMirror Reference to the object to copy
	 * @return none
	 **/
	XMLMirror(const XMLMirror& rMirror);

	/**
	 * Destructor for an XMLMirror object
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLMirror();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param mirror The mirror to get the values from
	 * @return The modified object
	 **/
	XMLMirror operator=(XMLMirror mirror);

//
// public member functions
//
public:
	/**
	 * Converts an XMLMirror object to a RPM spec file
	 * .
	 * @param rOut File stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Converts an XMLMirror object to an XML spec file
	 * .
	 * @param rOut File stream
	 * @return none
	 **/
	void toXMLFile(ostream& rOut);

//
// member variable get/set functions
//
public:
	/**
	 * Checks if we have a path
	 * .
	 * @param none
	 * @return true if we have a path, false otherise
	 **/
	bool hasPath()
	{
		return m_sPath.length() ? true : false;
	}

	/**
	 * Returns the path
	 * .
	 * @param none
	 * @return The path strinbg
	 **/
	const char* getPath()
	{
		return m_sPath.c_str();
	}

	/**
	 * Checks if we have a description set
	 * .
	 * @param none
	 * @return true is we have a description, false otherwise
	 **/
	bool hasDescription()
	{
		return m_sDescription.length() ? true : false;
	}

	/**
	 * Returns the description
	 * .
	 * @param none
	 * @return the description string
	 **/
	const char* getDescription()
	{
		return m_sDescription.c_str();
	}

	/**
	 * Checks if we have a country set
	 * .
	 * @param none
	 * @return true if we have a country, false otherwise
	 **/
	bool hasCountry()
	{
		return m_sCountry.length() ? true : false;
	}

	/**
	 * Gets the country
	 * .
	 * @param none
	 * @return The country string
	 **/
	const char* getCountry()
	{
		return m_sCountry.c_str();
	}

//
// member variables
//
protected:
	string     m_sPath;
	string     m_sDescription;
	string     m_sCountry;
};

#endif
