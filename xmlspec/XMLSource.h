#ifndef _H_XMLSOURCE_
#define _H_XMLSOURCE_

// standard C++ includes
#include <string>
#include <vector>
#include <iostream>

// standard includes
#include <stdio.h>

// our includes
#include "XMLAttrs.h"
#include "XMLBase.h"
#include "XMLMirror.h"

// rpm includes
#include <rpmbuild.h>

// forward declaration
class XMLSpec;

using namespace std;

// <source ...>
class XMLSource : public XMLBase
{
//
// factory functions
//
public:
	/**
	 * Static factory function for the creation of XMLSource, XMLPatch, ...
	 * objects from RPM Source* structure.
	 * .
	 * @param pSource Pointer to a list of sources
	 * @param pSpec pointer to the RPM spec
	 * @param pSpec pointer to our spec object
	 * @return true on success, false otherwise
	 **/
	static bool structCreate(Source* pSource,
							 Spec pSpec,
							 XMLSpec* pXSpec);

	/**
	 * Static factory function for the creation of an XMLSource
	 * object from details parsed from an XML spec.
	 * .
	 * @param pAttrs Pointer to an XML attribute object
	 * @param pspec Ponter to our spec object
	 * @param bPatch True if this source is a patch
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
	 * @param szName The name
	 * @param szNum The source number
	 * @param szDir The unpack directory
	 * @param szSize The size of the source archive
	 * @param szMD5 The MD5 sum of the archive
	 * @return none
	 **/
	XMLSource(const char* szName,
			  unsigned int nNum,
			  const char* szDir,
			  const char* szSize,
			  const char* szMD5);

	/**
	 * Copy contructor
	 * .
	 * @param rSource The source that we are to copy
	 * @return none
	 **/
	XMLSource(const XMLSource& rSource);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	virtual ~XMLSource();

//
// operators
//
public:
	/**
	 * Assignment operator
	 * .
	 * @param source The source to copy
	 * @return copied object
	 **/
	XMLSource operator=(XMLSource source);

//
// Member functions
//
public:
	/**
	 * Convert the object into an RPM spec file
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	virtual void toSpecFile(ostream& rOut);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	virtual void toXMLFile(ostream& rOut);

	/**
	 * Converts the object into an RPM structure
	 * .
	 * @param spec the RPM structure to use
	 * @return none
	 **/
	virtual void toRPMStruct(Spec spec);

//
// member variable get/set functions
//
public:
	/**
	 * Checks if we have a source name
	 * .
	 * @param none
	 * @return true if we have a name, false otherwise
	 **/
	bool hasName()
	{
		return m_sName.length() ? true : false;
	}

	/**
	 * Get the source name
	 * .
	 * @param none
	 * @return string containing the name
	 **/
	const char* getName()
	{
		return m_sName.c_str();
	}

	/**
	 * Get the length of the name
	 * .
	 * @param none
	 * @return the length of the name
	 **/
	unsigned int getNameLen()
	{
		return m_sName.length();
	}

	/**
	 * Get the source number
	 * .
	 * @param none
	 * @return the number
	 **/
	unsigned int getNum()
	{
		return m_nNum;
	}

	/**
	 * Checks to see if we have an unpack directory
	 * .
	 * @param none
	 * @return true if we have a specified directory, false otherwise
	 **/
	bool hasDir()
	{
		return m_sDir.length() ? true : false;
	}

	/**
	 * Gets the directory that we are to unpack this source to
	 * .
	 * @param none
	 * @return string contating the directory
	 **/
	const char* getDir()
	{
		return m_sDir.c_str();
	}

	/**
	 * Checks to see if we have a source size
	 * .
	 * @param none
	 * @return true if we have a size, false otherwise
	 **/
	bool hasSize()
	{
		return m_sSize.length() ? true : false;
	}

	/**
	 * Gets the size of the source
	 * .
	 * @param none
	 * @return string contating the size
	 **/
	const char* getSize()
	{
		return m_sSize.c_str();
	}

	/**
	 * Checks to see if this source has an MD5 sum
	 * .
	 * @param none
	 * @return true if we have an MD5, false oterwise
	 **/
	bool hasMD5()
	{
		return m_sMD5.length() ? true : false;
	}

	/**
	 * Gets the MD5 sum for this source
	 * .
	 * @param none
	 * @return string contating the MD5
	 **/
	const char* getMD5()
	{
		return m_sMD5.c_str();
	}

	/**
	 * Add a mirror for this source
	 * .
	 * @param rMirror The mirror to add
	 * @return none
	 **/
	void addMirror(XMLMirror& rMirror)
	{
		m_vMirrors.push_back(rMirror);
	}

	/**
	 * Gets the number of mirrors for this source
	 * .
	 * @param none
	 * @return the number oif mirrors
	 **/
	unsigned int numMirrors()
	{
		return m_vMirrors.size();
	}

	/**
	 * Gets a specific mirror by number
	 * .
	 * @param nNum The mirror to get
	 * @param the mirror
	 **/
	XMLMirror& getMirror(unsigned int nNum)
	{
		return m_vMirrors[nNum];
	}

//
// member variables
//
protected:
	string            m_sName;
	unsigned int      m_nNum;
	string            m_sDir;
	string            m_sSize;
	string            m_sMD5;
	vector<XMLMirror> m_vMirrors;
};

// <nosource ...>
class XMLNoSource : public XMLSource
{
//
// factory methods
//
public:
	/**
	 * Create an XMLNoSource object
	 * .
	 * @param pAttrs XML attributes
	 * @param pSpec The spec to add the object o
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
	 * @param szName The name
	 * @param nNum source number
	 * @param szDir Thje director to extract to
	 * @param szSize size of the archive
	 * @param szMD5 the MD5 sum of the archive
	 * @return none
	 **/
	XMLNoSource(const char* szName,
				unsigned int nNum,
				const char* szDir,
				const char* szSize,
				const char* szMD5);

	/**
	 * Copy constructor
	 * .
	 * @param rNoSource Reference to the object ot copy
	 * @return none
	 **/
	XMLNoSource(const XMLNoSource& rNoSource);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return none
	 **/
	virtual ~XMLNoSource();

//
// public member functions
//
public:
	/**
	 * Converts the object into an RPM spec file
	 * .
	 * @param rOut Output stream
	 * @param none
	 **/
	virtual void toSpecFile(ostream& rOut);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	virtual void toXMLFile(ostream& rOut);

	/**
	 * Converts the object into an RPM structure
	 * .
	 * @param spec The RPM spec structure
	 * @return none
	 **/
	 virtual void toRPMStruct(Spec spec);
};

// <patch ...>
class XMLPatch : public XMLSource
{
//
// factory methods
//
public:
	/**
	 * Create an XMLPatch object
	 * .
	 * @param pAttrs XML attributes
	 * @param pSpec The spec to add the object o
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
	 * @param szName archive name
	 * @param nNum source number
	 * @param szSize size of the archive
	 * @param szMD5 MD5 sum
	 * @return none
	 **/
	XMLPatch(const char* szName,
			 unsigned int nNum,
			 const char* szSize,
			 const char* szMD5);

	/**
	 * Copy constructor
	 * .
	 * @param rPatch Reference to the object to copy
	 * @return none
	 **/
	XMLPatch(const XMLPatch& rPatch);

	/**
	 * Destructor
	 * .
	 * @param none
	 * @return nonew
	 **/
	virtual ~XMLPatch();

//
// member functions
//
public:
	/**
	 * Converts the object into an RPM spec file
	 * .
	 * @param rOut Output stream
	 * @param none
	 **/
	virtual void toSpecFile(ostream& rOut);

	/**
	 * Converts the object into an XML spec
	 * .
	 * @param rOut Output stream
	 * @return none
	 **/
	virtual void toXMLFile(ostream& rOut);

	/**
	 * Converts the object into an RPM structure
	 * .
	 * @param spec The RPM spec structure
	 * @return none
	 **/
	virtual void toRPMStruct(Spec spec);
};

#endif
