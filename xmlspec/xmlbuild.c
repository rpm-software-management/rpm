#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "stringbuf.h"
#include "rpmbuild.h"

#include "xml2rpm.h"
#include "xmlbuild.h"
#include "xmlparse.h"
#include "xmlstruct.h"

extern Spec g_pSpec;

int buildXMLSpec(t_structXMLSpec* pXMLSpec,
		 int nWhat,
		 int nTest)
{
	Spec pRPMSpec = NULL;
	int nRet = 0;

	if ((pRPMSpec = toRPMStruct(pXMLSpec))) {
		nRet = buildSpec(pRPMSpec, nWhat, nTest);
		if (g_pSpec)
			freeSpec(g_pSpec);
		g_pSpec = NULL;
	}
	else
		nRet = -1;
		
	return nRet;
}

int parseBuildXMLSpec(const char* szXMLSpec,
		      int nWhat,
		      int nTest,
		      int nVerbose)
{
	t_structXMLSpec* pXMLSpec = NULL;
	int nRet = 0;

	if ((pXMLSpec = parseXMLSpec(szXMLSpec, nVerbose))) {
		nRet = buildXMLSpec(pXMLSpec, nWhat, 0);
		freeXMLSpec(&pXMLSpec);
	}
	else
		nRet = -1;

	return nRet;
}
