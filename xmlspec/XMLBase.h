#ifndef _H_XMLBASE_
#define _H_XMLBASE_

// standard C++ includes
#include <string>

// standard include
#include <stdio.h>

using namespace std;

class XMLBase
{
//
// constructors/destructor methods
//
public:
	/**
	 * Default class constructor
	 * .
	 * param none
	 * @return none
	 **/
	XMLBase()
	{
		m_bHasError = false;
		m_bHasWarning = false;
	}

	/**
	 * Default class destructor
	 * .
	 * param none
	 * @return none
	 **/
	~XMLBase()
	{
	}

//
// member variable get and set functions
//
public:
	/**
	 * Tests if the object has an error set.
	 * .
	 * @param none
	 * @return true if we have an error, false otherwise
	 **/
	bool hasError()
	{
		return m_bHasError;
	}

	/**
	 * Sets the error message that can be retrieved from this
	 * object instance
	 * .
	 * @param szError The error string
	 * @return none
	 **/
	void setError(const char* szError = NULL)
	{
		if (szError) {
			m_bHasError = true;
			m_sError.assign(szError);
		}
		else
			m_bHasError = false;
	}

	/**
	 * Returns the currently set error value
	 * .
	 * @param none
	 * @return pointer to the error string
	 **/
	const char* getError()
	{
		m_bHasError = false;
		return m_sError.c_str();
	}

	/**
	 * Tests if the object has a warning set.
	 * .
	 * @param none
	 * @return true if we have a warning, false otherwise
	 **/
	bool hasWarning()
	{
		return m_bHasWarning;
	}

	/**
	 * Sets the warning message that can be retrieved from this
	 * object instance
	 * .
	 * @param szWarning The warning string
	 * @return none
	 **/
	void setWarning(const char* szWarning = NULL)
	{
		if (szWarning) {
			m_bHasWarning = true;
			m_sWarning.assign(szWarning);
		}
		else
			m_bHasWarning = false;
	}

	/**
	 * Returns the currently set warning value
	 * .
	 * @param none
	 * @return pointer to the warning string
	 **/
	const char* getWarning()
	{
		m_bHasWarning = false;
		return m_sWarning.c_str();
	}

//
// protected internal variables
//
protected:
	bool   m_bHasError;
	string m_sError;
	bool   m_bHasWarning;
	string m_sWarning;
};

#endif
