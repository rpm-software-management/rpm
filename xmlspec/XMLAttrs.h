#ifndef _H_XMLATTRS_
#define _H_XMLATTRS_

// standard C++ includes
#include <string>
#include <vector>

// standard C includes
#include <string.h>

// our includes
#include "XMLBase.h"

using namespace std;

// definition for the end of the attributes
#define XATTR_END        0xFFFF
#define XATTR_NUM_VALSTR 5

enum eAttrType
{
	XATTRTYPE_STRING = 0,
	XATTRTYPE_INTEGER,
	XATTRTYPE_BOOL,
	XATTRTYPE_DATE,
	XATTRTYPE_MAIL,
	XATTRTYPE_NONE = XATTR_END
};

struct structValidAttrs
{
	unsigned int m_nValue;
	bool         m_bMandatory;
	bool         m_bFound;
	char*        m_szName;
	unsigned int m_nType;
	char*        m_szaMatches[XATTR_NUM_VALSTR+1];
};

// forward class definitions
class XMLSpec;

class XMLAttr : public XMLBase
{
//
// contructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szName The name of the attribute
	 * @param szValue The attribute value
	 * @return none
	 **/
	XMLAttr(const char* szName,
			const char* szValue);

	/**
	 * Copy constructor
	 * .
	 * @param rAttr Reference to the attribute to copy
	 * @return none
	 **/
	XMLAttr(const XMLAttr& rAttr);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLAttr();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param attr The attribute to copy
	 * @return the assigned obkect
	 **/
	XMLAttr operator=(XMLAttr attr);

//
// get/set functions
//
public:
	/**
	 * Returns the attribute name
	 * .
	 * @param none
	 * @return string containing the attribute name
	 **/
	const char* getName()
	{
		return m_sName.c_str();
	}

	/**
	 * Returns the attribute value (as string)
	 * .
	 * @param none
	 * @return string containing the attribute value
	 **/
	const char* asString()
	{
		return m_sValue.c_str();
	}

	/**
	 * Returns the attribute value (as integer)
	 * .
	 * @param none
	 * @return the attribute as an integer
	 **/
	unsigned int asInteger()
	{
		return atoi(m_sValue.c_str());
	}

	/**
	 * Returns the attribute value as a boolean
	 * .
	 * @param none
	 * @return true if set, false otherwise
	 **/
	bool asBool()
	{
		bool isSet = true;
		if (strcasecmp(m_sValue.c_str(), "no") == 0 ||
			strcasecmp(m_sValue.c_str(), "0") == 0 ||
			strcasecmp(m_sValue.c_str(), "false") == 0)
			isSet = false;
		return isSet;
	}

//
// member variables
//
public:
	string m_sName;
	string m_sValue;
};

class XMLAttrs : public XMLBase
{
//
// constructors/destructor
//
public:
	/**
	 * The default attribute constructor
	 * .
	 * @param szAttrs Pointer to an array of attributes, terminated by NULL
	 * @return none
	 **/
	XMLAttrs(const char** szAttrs);

	/**
	 * Copy constructor
	 * .
	 * @param rAttrs The attribute object to copy
	 * @return none
	 **/
	 XMLAttrs(const XMLAttrs& rAttrs);

	/**
	 * The default destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLAttrs();

//
// member functions
//
public:
	/**
	 * Validates an attribute object against the valid attributes. This
	 * checks for mandatory attributes (error) as well as unexpected
	 * attributes. (warnings)
	 * .
	 * @param paValids Pointer to the array of valid attributes
	 * @param pError   The class in which we will set the errors
	 *                 and/or warnings.
	 * @return true on valid attributes, false otherwise
	 **/
	bool validate(structValidAttrs* paValids,
				  XMLBase* pError);

//
// member variables get/set functions
//
public:
	/**
	 * Gets the number of attributes contained in this object
	 * .
	 * @param none
	 * @returns The number of attributes
	 **/
	unsigned int num()
	{
		return m_vAttrs.size();
	}

	/**
	 * Returns a specific attribute by number
	 * .
	 * @param nNum The number of the attribute to return
	 * @return The attribute or NULL if it doesn't exist
	 **/
	XMLAttr& get(unsigned int nNum)
	{
		return m_vAttrs[nNum];
	}

	/**
	 * Returns the attribute as specified by the name
	 * .
	 * @param szName The name of the attribute whose value we are
	 *               to return
	 * @return The attribute as a string
	 **/
	const char* asString(const char* szName);

	/**
	 * Returns the attribute as specified by the name
	 * .
	 * @param szName The name of the attribute whose value we are
	 *               to return
	 * @return The attribute as an integer
	 **/
	unsigned int asInteger(const char* szName);

	/**
	 * Returns the attribute as specified by the name
	 * .
	 * @param szName The name of the attribute whose value we are
	 *               to return
	 * @return The attribute as a bool
	 **/
	bool asBool(const char* szName);

//
// protected data members
//
protected:
	vector<XMLAttr> m_vAttrs;
};

#endif
