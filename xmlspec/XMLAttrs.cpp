// standard C++ includes
#include <string>

// standard includes
#include <string.h>

// our includes
#include "XMLAttrs.h"
#include "XMLSpec.h"

using namespace std;

XMLAttr::XMLAttr(const char* szName,
				const char* szValue)
	: XMLBase()
{
	if (szName)
		m_sName.assign(szName);
	if (szValue)
		m_sValue.assign(szValue);
}

XMLAttr::XMLAttr(const XMLAttr& rAttr)
	: XMLBase()
{
	m_sName.assign(rAttr.m_sName);
	m_sValue.assign(rAttr.m_sValue);
}

XMLAttr::~XMLAttr()
{
}

XMLAttr XMLAttr::operator=(XMLAttr attr)
{
	m_sName.assign(attr.m_sName);
	m_sValue.assign(attr.m_sValue);
}

XMLAttrs::XMLAttrs(const char** szAttrs)
	: XMLBase()
{
	for (int i = 0; szAttrs && szAttrs[i]; i += 2) {
		XMLAttr attr(szAttrs[i],
					szAttrs[i+1]);
		m_vAttrs.push_back(attr);
	}
}

XMLAttrs::XMLAttrs(const XMLAttrs& rAttrs)
	: XMLBase()
{
	m_vAttrs = rAttrs.m_vAttrs;
}

XMLAttrs::~XMLAttrs()
{
}

bool XMLAttrs::validate(structValidAttrs* paValids,
						XMLBase* pError)
{
	// nothing found at present
	for (unsigned int i = 0; paValids[i].m_nValue != XATTR_END; i++)
		paValids[i].m_bFound = false;

	// test everything we have
	for (unsigned int i = 0; i < num(); i++) {
		bool bInvalid = true;
		for (unsigned int j = 0; paValids[j].m_nValue != XATTR_END; j++) {
			if (strcasecmp(paValids[j].m_szName, get(i).getName()) == 0) {
				paValids[i].m_bFound = true;
				bInvalid = false;
				break;
			}
		}
		if (bInvalid) {
			char szTmp[1024];
			sprintf(szTmp, "Unknown attribute '%s'", get(i).getName());
			pError->setWarning(szTmp);
		}
	}

	// see if we have mandator tags that are not there
	for (unsigned int i = 0; paValids[i].m_nValue != XATTR_END; i++) {
		if (paValids[i].m_bMandatory && !paValids[i].m_bFound) {
			char szTmp[1024];
			sprintf(szTmp, "Mandatory attribute '%s' not found",
							paValids[i].m_szName);
			pError->setError(szTmp);
			return false;
		}
	}

	// if we got this far, everything is ok
	return true;
}

const char* XMLAttrs::get(const char* szName)
{
	if (szName)
	{
		for (unsigned int i = 0; i < num(); i++) {
			if (strcasecmp(szName, get(i).getName()) == 0)
				return get(i).getValue();
		}
	}
	return NULL;
}
