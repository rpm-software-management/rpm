// standard C++ includes
#include <string>

// standard includes
#include <string.h>

// our includes
#include "XMLAttrs.h"
#include "XMLMisc.h"
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
		XMLAttr attr(szAttrs[i], szAttrs[i+1]);
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

bool validateAttr(structValidAttrs& rAttr,
				  const char* szValue)
{
	switch (rAttr.m_nType) {
		case XATTRTYPE_STRING:
			return findStr(rAttr.m_szaMatches, szValue) != -1 ? true : false;
			break;
		case XATTRTYPE_INTEGER:
			return isInteger(szValue);
			break;
		case XATTRTYPE_BOOL:
			return isBool(szValue);
			break;
		case XATTRTYPE_DATE:
			return isDate(szValue);
			break;
		case XATTRTYPE_MAIL:
			return isEmail(szValue);
			break;
		case XATTRTYPE_NONE:
		default:
			return true;
			break;
	}
	return false;
}

static char* szaTypeDesc[] = { "string",
							   "integer",
							   "bool [Values: true|false]",
							   "date [Format: DDD MMM DD YYYY]",
							   "e-mail" };

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
				paValids[j].m_bFound = true;
				if (!validateAttr(paValids[j], get(i).asString())) {
					char szTmp[1024];
					sprintf(szTmp, "Attribute value '%s' is not a valid %s.",
									get(i).asString(), szaTypeDesc[paValids[j].m_nType]);
					pError->setError(szTmp);
					return false;
				}
				else
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

unsigned int getPos(const char* szName,
					XMLAttrs* pAttrs)
{
	if (szName) {
		for (unsigned int i = 0; i < pAttrs->num(); i++) {
			if (strcasecmp(szName, pAttrs->get(i).getName()) == 0)
				return i;
		}
	}
	return XATTR_END;
}

const char* XMLAttrs::asString(const char* szName)
{
	unsigned int nPos = getPos(szName, this);
	return (nPos == XATTR_END) ? NULL : get(nPos).asString();
}

unsigned int XMLAttrs::asInteger(const char* szName)
{
	unsigned int nPos = getPos(szName, this);
	return (nPos == XATTR_END) ? 0 : get(nPos).asInteger();
}

bool XMLAttrs::asBool(const char* szName)
{
	unsigned int nPos = getPos(szName, this);
	return (nPos == XATTR_END) ? true : get(nPos).asBool();
}
