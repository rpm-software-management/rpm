#ifndef _H_XMLTEXT_
#define _H_XMLTEXT_

// standard c++ includes
#include <string>

// our includes
#include "XMLBase.h"

using namespace std;

class XMLText : public XMLBase
{
//
// constructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szText The text to add
	 * @param szLang The text's language
	 * @return none
	 **/
	XMLText(const char* szText,
			const char* szLang = NULL);

	/**
	 * Copy constructor
	 * .
	 * @param rText reference to the text to copy
	 * @return none
	 **/
	XMLText(const XMLText& rText);

	/**
	 * Default destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLText();

//
// get/set methods for internal variables
//
public:
	/**
	 * Sets the text
	 * .
	 * @param szText The text
	 * @return none
	 **/
	void setText(const char* szText)
	{
		if (szText)
			m_sText.assign(szText);
	}

	/**
	 * Gets the text
	 * .
	 * @param none
	 * @return string containing the text
	 **/
	const char* getText()
	{
		return m_sText.c_str();
	}

	/**
	 * Tests if we have a language for this description
	 * .
	 * @param none
	 * @return true if we have a language, false otherwise
	 **/
	bool hasLang()
	{
		return m_sLang.length() ? true : false;
	}

	/**
	 * Sets the language
	 * .
	 * @param szLang The language
	 * @return none
	 **/
	void setLang(const char* szLang)
	{
		if (szLang) {
			// FIXME: We need to get the actual default language as specified
			// in one of the RPM headers (which I cannot find now) and
			// substitute it here. (I know the value is "C", bu we should
			// use the define.)
			if (strcmp(szLang, "C") != 0)
				m_sLang.assign(szLang);
		}
	}

	/**
	 * Gets the language
	 * .
	 * @param none
	 * @return string containing the language
	 **/
	const char* getLang()
	{
		return m_sLang.c_str();
	}

//
// member variables
//
public:
	string m_sText;
	string m_sLang;
};

#endif
