#ifndef _H_XMLCHANGELOG_
#define _H_XMLCHANGELOG_

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
class XMLSpec;
class XMLChangelog;

using namespace std;

class XMLChangelogEntry : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Adds a changelog entry
	 * .
	 * @param pAttrs  The XML attributes
	 * @param szEntry The entry to create
	 * @param pSpec   Pointer to our spec
	 * @return true on success, false othersise
	 */
	static bool parseCreate(XMLAttrs* pAttrs,
							const char* szEntry,
							XMLSpec* pSpec);

//
// constructors/destructor
//
public:
	/**
	 * Default contructor
	 * .
	 * @param szChange The change entry
	 * @return none
	 **/
	 XMLChangelogEntry(const char* szChange);

	 /**
	  * Copy constructor
	  * .
	  * @param rEntry Reference to the entry to copy
	  * @return none
	  **/
	  XMLChangelogEntry(const XMLChangelogEntry& rEntry);

	 /**
	  * Destructor
	  * .
	  * @param none
	  * @return none
	  **/
	  ~XMLChangelogEntry();

//
// member functions
//
public:
	/**
	 * Outputs the object into a spec file
	 * .
	 * @param rOut Reference to our output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Outputs the object into an XML spec file
	 * .
	 * @param rOut Reference to our output stream
	 * @return none
	 **/
	 void toXMLFile(ostream& rOut);

//
// member variable get/set functions
//
public:
	/**
	 * Returns the enrty
	 * .
	 * @param none
	 * @return string containing the entry
	 **/
	 const char* getChange()
	 {
	 	return m_sChange.c_str();
	 }

//
// member variables
//
public:
	string m_sChange;
};

class XMLChangelogDate : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Creates a XMLChangelogDate object
	 * .
	 * @param pAttrs The XML attributes
	 * @param pSpec The spec to add this object to
	 * @return true on success, false otherwise
	 **/
	static bool parseCreate(XMLAttrs* pAttrs,
							XMLSpec* pSpec);

//
// constructors/destructor
//
public:
	/**
	 * Default contructor
	 * .
	 * @param szDate The date
	 * @param szAuthor The author
	 * @param szEmail The author's email
	 * @param szVersion The version in which this change was made
	 * @return none
	 **/
	XMLChangelogDate(const char* szDate,
					 const char* szAuthor,
					 const char* szEmail,
					 const char* szVersion);

	/**
	 * Copy contructor
	 * .
	 * @param rDate Reference to the date object to copy
	 * @return none
	 **/
	XMLChangelogDate(const XMLChangelogDate& rDate);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLChangelogDate();

//
// public member functions
//
public:
	/**
	 * Outputs the object into a spec file
	 * .
	 * @param rOut Reference to our output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Outputs the object into an XML spec file
	 * .
	 * @param rOut Reference to our output stream
	 * @return none
	 **/
	 void toXMLFile(ostream& rOut);

//
// member variables get/set functions
//
public:
	/**
	 * Returns the date for the group of entries
	 * .
	 * @param none
	 * @return string representation of the date
	 **/
	const char* getDate()
	{
		return m_sDate.c_str();
	}

	/**
	 * Checks for an author
	 * .
	 * @param none
	 * @return true if we have an author, false otherwise
	 **/
	bool hasAuthor()
	{
		return m_sAuthor.length() ? true : false;
	}

	/**
	 * Returns the author's name
	 * .
	 * @param none
	 * @return string containing the author's name
	 **/
	const char* getAuthor()
	{
		return m_sAuthor.c_str();
	}

	/**
	 * Checks if we have an email address for the author
	 * .
	 * @param none
	 * @return true if we hava an email, false otherwise
	 **/
	bool hasEmail()
	{
		return m_sEmail.length() ? true : false;
	}

	/**
	 * Returns the author's email addresse
	 * .
	 * @param none
	 * @return a string containing the author's email address
	 **/
	const char* getEmail()
	{
		return m_sEmail.c_str();
	}

	/**
	 * Checks if we have a change version
	 * .
	 * @param none
	 * @return true if we have a version, false otherwise
	 **/
	bool hasVersion()
	{
		return m_sVersion.length() ? true : false;
	}

	/**
	 * Gets the change version
	 * .
	 * @param none
	 * @return string containing the version
	 **/
	const char* getVersion()
	{
		return m_sVersion.c_str();
	}

	/**
	 * Returns the number of entries for this date
	 * .
	 * @param none
	 * @return the number of entries
	 **/
	unsigned int numEntries()
	{
		return m_vEntries.size();
	}

	/**
	 * Returns a specific entry
	 * .
	 * @param nNum The number of the entry to return
	 * @return the enrty
	 **/
	XMLChangelogEntry& getEntry(unsigned int nNum)
	{
		return m_vEntries[nNum];
	}

	/**
	 * Adds an entry for this date
	 * .
	 * @param szEntry The entry to add
	 * @return none
	 **/
	void addEntry(XMLChangelogEntry& rEntry)
	{
		m_vEntries.push_back(rEntry);
	}

//
// member variables
//
protected:
	string                    m_sDate;
	string                    m_sAuthor;
	string                    m_sEmail;
	string                    m_sVersion;
	vector<XMLChangelogEntry> m_vEntries;
};

class XMLChangelog : public XMLBase
{
//
// static factory functions
//
public:
	/**
	 * Creates changelog objects from an RPM Spec structure
	 * .
	 * @param pSpec Pointer to the RPM spec
	 * @param pXSpec pointer to the XMLSpec object to populate
	 * @return true on success, false otherwise
	 **/
	static bool structCreate(Spec pSpec,
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
	XMLChangelog();

	/**
	 * Copy constructor
	 * .
	 * @param rChangelog The object to copy
	 * @return none
	 **/
	XMLChangelog(const XMLChangelog& rChangelog);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLChangelog();

//
// public member functions
//
public:
	/**
	 * Converts the object into a spec file
	 * .
	 * @param rOut Reference to the output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Converts the object into an xML spec
	 * .
	 * @param rOut Reference to the output stream
	 * @return none
	 **/
	 void toXMLFile(ostream& rOut);

	 /**
	  * Converts the object into an RPM structure
	  * .
	  * @param spec RPM structure
	  * @return none
	  **/
	  void toRPMStruct(Spec spec);
//
// variable get/set functions
//
public:
	/**
	 * Adds a date to the changelog
	 * .
	 * @param rDate The date to add
	 * @return none
	 **/
	void addDate(XMLChangelogDate& rDate)
	{
		m_vDates.push_back(rDate);
	}

	/**
	 * Returns the number of dates in the changelog
	 * .
	 * @param none
	 * @return the number of dates
	 **/
	unsigned int numDates()
	{
		return m_vDates.size();
	}

	/**
	 * Gets a specific date
	 * .
	 * @param nNum The entry number
	 * @return The requated date
	 **/
	XMLChangelogDate& getDate(unsigned int nNum)
	{
		return m_vDates[nNum];
	}

	/**
	 * Gets the last date we have added
	 * .
	 * @param none
	 * @return the last date
	 **/
	XMLChangelogDate& lastDate()
	{
		return m_vDates[numDates()-1];
	}

//
// member variables
//
protected:
	vector<XMLChangelogDate> m_vDates;
};

#endif
