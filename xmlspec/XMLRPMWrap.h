#ifndef _H_XMLRPMWRAP_
#define _H_XMLRPMWRAP_

// standard C++ includes
#include <string>
#include <vector>

// rpm includes
#include <rpmlib.h>

// type definitions
typedef vector<string> t_StrVector;

/**
 * Gets an RPM header after checking that is does exist.
 * .
 * @param header The RPM header to interrogate
 * @param nTag The header tag to extract
 * @param rResult The string to populate with the result
 * @return true on success, false otherwise
 **/
extern bool getRPMHeader(Header header,
						 int_32 nTag,
						 std::string& rResult);

/**
 * Gets an RPM header array into a vector after checking that it
 * does indeed exist
 * .
 * @param header The RPM header to interrogate
 * @param nTag The header tag to extract
 * @param rvResult The vector<string> to populate with the result
 * @return true on success, false otherwise
 **/
extern bool getRPMHeaderArray(Header header,
							  int_32 nTag,
							  t_StrVector& rvResult);

/**
 * Gets a specific RPM macro
 * .
 * @param szMacro The macro to get the value of
 * @param rResult The string to populate with the result
 * @return true on success, false otherwise
 **/
extern bool getRPMMacro(const char* szMacro,
						std::string& rResult);

#endif
