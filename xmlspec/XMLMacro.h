#ifndef _H_XMLMACRO_
#define _H_XMLMACRO_

// standard C++ includes
#include <iostream>
#include <string>

// our includes
#include "XMLAttrs.h"

// forward class definitions
class XMLSpec;

using namespace std;

//<macro ...>
class XMLMacro : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Creates an XMLMacro object
	 * .
	 * @param pAttrs XML tag attributes
	 * @param szMacro The macro contents
	 * @param pSpec the spec to add the macro to
	 * @return true on success, false otherwise
	 **/
	static bool parseCreate(XMLAttrs* pAttrs,
							const char* szMacro,
							XMLSpec* pSpec);

//
// constructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szName The name of the macro
	 * @param szMacro The expanded macro
	 * @return none
	 **/
	XMLMacro(const char* szName,
			 const char* szMacro);

	/**
	 * Copy constructor
	 * .
	 * @param rMacro the macro to copy
	 * @return none
	 **/
	XMLMacro(const XMLMacro& rMacro);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLMacro();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param macro The macro to copy
	 * @return our copied object
	 **/
	XMLMacro operator=(XMLMacro macro);

//
// member functions
//
public:
	/**
	 * Outputs the macro into an RPM spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Outputs the macro into an XML spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toXMLFile(ostream& rOut);

//
// member get/set functions
//
public:
	/**
	 * Gets the macro name
	 * .
	 * @param none
	 * @return string containing the macro name
	 **/
	const char* getName()
	{
		return m_sName.c_str();
	}

	/**
	 * Gets tha macro value
	 * .
	 * @param none
	 * @return string contatining the macro value
	 **/
	const char* getValue()
	{
		return m_sValue.c_str();
	}

//
// member variables
//
protected:
	string m_sName;
	string m_sValue;
};

#endif
