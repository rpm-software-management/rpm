#ifndef _H_XMLSCRIPT_
#define _H_XMLSCRIPT_

// standard C++ includes
#include <iostream>
#include <string>
#include <vector>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"

// rpm includes
#include <rpmbuild.h>

// forward definitions
class XMLPackage;
class XMLSpec;
class XMLScripts;

using namespace std;

class XMLScript : public XMLBase
{
//
// factory methods
//
public:
	/**
	 * Creates a script object and adds it to the container
	 * .
	 * @param pAttrs The XML attributes
	 * @param szScript The script value
	 * @param rContainer reference to the script container to add to
	 * @return true on success, false otherwise
	 **/
	 static bool parseCreate(XMLAttrs* pAttrs,
							 const char* szScript,
							 XMLScripts& rContainer);

//
// constructors/destructor
//
public:
	/**
	 * Default constructor
	 * .
	 * @param szScript The script
	 * @param szInterpreter The interpreter to use for script execution
	 * @param szDir Directory to execute the script in
	 * @return none
	 **/
	 XMLScript(const char* szScript,
	 		   const char* szInterpreter,
			   const char* szDir);

	/**
	 * Copy constructor
	 * .
	 * @param rScript Script to copy
	 * @return none
	 **/
	 XMLScript(const XMLScript& rScript);

	 /**
	  * Destructor
	  * .
	  * @param none
	  * @return none
	  **/
	  virtual ~XMLScript();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param script the script to copy
	 * @return the copied object
	 **/
	 XMLScript operator=(XMLScript script);

//
// member functions
//
public:
	/**
	 * Converts the object into an RPM spec file
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	void toSpecFile(ostream& rOut);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @param szIndent Indent string
	 * @return none
	 **/
	void toXMLFile(ostream& rOut,
				   const char* szIndent = "");

	/**
	 * Converts the object into an RPM structure
	 * .
	 * @param pSB Pointer to the string buffer
	 * @return none
	 **/
	virtual void toRPMStruct(StringBuf* pSB);

//
// member variable get/set functions
//
public:
	/**
	 * Gets the script value
	 * .
	 * @param none
	 * @return string containing the script
	 **/
	 const char* getValue()
	 {
	 	return m_sValue.c_str();
	 }

	/**
	 * Checks if we have an interpreter
	 * .
	 * @param none
	 * @return true if we have an interpreter, false otherwise
	 **/
	bool hasInterpreter()
	{
		return m_sInterpreter.length() ? true : false;
	}

	/**
	 * Gets the interpreter
	 * .
	 * @param none
	 * @return string contating the interpreter
	 **/
	const char* getInterpreter()
	{
		return m_sInterpreter.c_str();
	}

	/**
	 * Checks if we have a direcory
	 * .
	 * @param none
	 * @return true if we have a directory, false otherwise
	 **/
	bool hasDirectory()
	{
		return m_sDir.length() ? true : false;
	}

	/**
	 * Gets the directory
	 * .
	 * @param none
	 * @return string contating the directory
	 **/
	const char* getDirectory()
	{
		return m_sDir.c_str();
	}

	/**
	 * Gets the length of the directory string
	 * .
	 * @param none
	 * @return length of the description string
	 **/
	 unsigned int getDirectoryLen()
	 {
	 	return m_sDir.length();
	 }

//
// member variables
//
public:
	string m_sValue;
	string m_sInterpreter;
	string m_sDir;
};

//<prep,build,install,clean ...>
class XMLScripts : public XMLBase
{
//
// factory functions
//
public:
	static bool addPrepScript(XMLAttrs* pAttrs,
							  const char* szScript,
							  XMLSpec* pSpec);

	static bool addBuildScript(XMLAttrs* pAttrs,
							   const char* szScript,
							   XMLSpec* pSpec);

	static bool addInstallScript(XMLAttrs* pAttrs,
								 const char* szScript,
								 XMLSpec* pSpec);

	static bool addCleanScript(XMLAttrs* pAttrs,
							   const char* szScript,
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
	XMLScripts();

	/**
	 * Copy constructor
	 * .
	 * @param rContainer the object to copy
	 * @return none
	 **/
	 XMLScripts(const XMLScripts& rContainer);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	virtual ~XMLScripts();

//
// member functions
//
public:
	/**
	 * Converts the object into an RPM spec file
	 * .
	 * @param rOut Output stream
	 * @param szTag The tag name
	 * @return none
	 **/
	virtual void toSpecFile(ostream& rOut,
							const char* szTag);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @param szTag The tag name
	 * @return none
	 **/
	void toXMLFile(ostream& rOut,
				   const char* szTag);

//
// member variable get/set functions
//
public:
	/**
	 * Checks if we have an interpreter
	 * .
	 * @param none
	 * @return true if we have an interpreter, false otherwise
	 **/
	bool hasInterpreter()
	{
		return m_sInterpreter.length() ? true : false;
	}

	/**
	 * Gets the interpreter
	 * .
	 * @param none
	 * @return string contatining the interpreter
	 **/
	const char* getInterpreter()
	{
		return m_sInterpreter.c_str();
	}

	/**
	 * Sets the script interpreter
	 * .
	 * @param szInterpreter The interpreter
	 * @return none
	 **/
	void setInterpreter(const char* szInterpreter)
	{
		if (szInterpreter)
			m_sInterpreter.assign(szInterpreter);
	}

	/**
	 * Gets the number of script entries
	 * .
	 * @param none
	 * @return the number of scripts
	 **/
	unsigned int numScripts()
	{
		return m_vScripts.size();
	}

	/**
	 * Gets a specific script entry
	 * .
	 * @param nNum The entry number
	 * @return Reference to the script entry
	 **/
	XMLScript& getScript(unsigned int nNum)
	{
		return m_vScripts[nNum];
	}

	/**
	 * Adds an script entry
	 * .
	 * @param szScript the script to add
	 * @return none
	 **/
	void add(XMLScript& rScript)
	{
		m_vScripts.push_back(rScript);
	}

//
// member variables
//
protected:
	string            m_sInterpreter;
	vector<XMLScript> m_vScripts;
};

//<post, postun, ...>
class XMLPackageScripts : public XMLScripts
{
//
// factory functions
//
public:
	/**
	 * Adds a pre script
	 * .
	 * @param pAttrs The XML attributes
	 * @param szScript The script to add
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool addPreScript(XMLAttrs* pAttrs,
							 const char* szScript,
							 XMLSpec* pSpec);

	/**
	 * Adds a post script
	 * .
	 * @param pAttrs The XML attributes
	 * @param szScript The script to add
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool addPostScript(XMLAttrs* pAttrs,
							  const char* szScript,
							  XMLSpec* pSpec);

	/**
	 * Adds a preun script
	 * .
	 * @param pAttrs The XML attributes
	 * @param szScript The script to add
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool addPreUnScript(XMLAttrs* pAttrs,
							   const char* szScript,
							   XMLSpec* pSpec);

	/**
	 * Adds a postun script
	 * .
	 * @param pAttrs The XML attributes
	 * @param szScript The script to add
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool addPostUnScript(XMLAttrs* pAttrs,
								const char* szScript,
								XMLSpec* pSpec);

	/**
	 * Adds a verify script
	 * .
	 * @param pAttrs The XML attributes
	 * @param szScript The script to add
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool addVerifyScript(XMLAttrs* pAttrs,
								const char* szScript,
								XMLSpec* pSpec);

	/**
	 * Initialises a pre script container
	 * .
	 * @param pAttrs The XML attributes
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool createPreScripts(XMLAttrs* pAttrs,
								 XMLSpec* pSpec);

	/**
	 * Initialises a post script container
	 * .
	 * @param pAttrs The XML attributes
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool createPostScripts(XMLAttrs* pAttrs,
								  XMLSpec* pSpec);

	/**
	 * Initialises a preun script container
	 * .
	 * @param pAttrs The XML attributes
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool createPreUnScripts(XMLAttrs* pAttrs,
								   XMLSpec* pSpec);

	/**
	 * Initialises a postun script container
	 * .
	 * @param pAttrs The XML attributes
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool createPostUnScripts(XMLAttrs* pAttrs,
									XMLSpec* pSpec);

	/**
	 * Initialises a verify script container
	 * .
	 * @param pAttrs The XML attributes
	 * @param pSpec The spec to which we are adding
	 * @return true on success, false otherwise
	 **/
	static bool createVerifyScripts(XMLAttrs* pAttrs,
									XMLSpec* pSpec);

//
// constructors/destructors
//
public:
	/**
	 * Default constructor
	 * .
	 * @param none
	 * @return none
	 **/
	XMLPackageScripts();

	/**
	 * Copy constructor
	 * .
	 * @param rScripts Reference to the object to copy
	 * @return none
	 **/
	XMLPackageScripts(const XMLPackageScripts& rScripts);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	~XMLPackageScripts();

//
// member functions
//
public:
	/**
	 * Converts the object into an RPM spec
	 * .
	 * @param rOut Output stream
	 * @param szTag The tag name
	 * @return none
	 **/
	virtual void toSpecFile(ostream& rOut,
							const char* szTag);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @param szTag The tag name
	 * @return none
	 **/
	virtual void toXMLFile(ostream& rOut,
						   const char* szTag);

};

#endif
