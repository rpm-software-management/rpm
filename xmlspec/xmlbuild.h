#ifndef _XML_BUILD_H_
#define _XML_BUILD_H_

#include "xmlstruct.h"

int buildXMLSpec(t_structXMLSpec* pXMLSpec,
		 int nWhat,
		 int nTest);

int parseBuildXMLSpec(const char* szXMLSpec,
		      int nWhat,
		      int nTest,
		      int nVerbose);

#endif
