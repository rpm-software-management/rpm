#ifndef _H_XMLPARSER_
#define _H_XMLPARSER_

// our includes
#include "XMLSpec.h"

/**
 * Parses an XML spec into the internal data structures as
 * defined in the XML* classes.
 * .
 * @param szXMLFilename The Spec to read as parser input
 * @param pSpec         A reference to the spec data structure we
 *                      are to fill.
 * @return The number of parsing/other errors (0 == success)
 **/
extern int parseXMLSpec(const char* szXMLFilename,
						XMLSpec*& pSpec);

#endif
