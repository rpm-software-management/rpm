// standard C++ includes
#include <string>

// our includes
#include "XMLChangelog.h"
#include "XMLMisc.h"
#include "XMLRPMWrap.h"
#include "XMLSpec.h"

using namespace std;

bool XMLChangelogEntry::parseCreate(XMLAttrs* pAttrs,
									const char* szChange,
									XMLSpec* pSpec)
{
	if (!pSpec)
		return false;
	XMLChangelogEntry change(szChange);
	pSpec->getChangelog().lastDate().addEntry(change);
	return true;
}

bool XMLChangelogEntry::structCreate(const char* szEntries,
									 XMLSpec* pXSpec)
{
	if (!pXSpec || !szEntries)
		return false;
	char* szIn = (char*)szEntries;
	char* szOut = NULL;
	int nLen = -1;
	string sChange;
	while (nLen != 0) {
		szOut = splitStr(szIn, '\n', nLen);
		if (strncmp(szIn, "- ", 2) == 0) {
			szIn += 2;
			nLen -= 2;
			if (sChange.length()) {
				XMLChangelogEntry change(sChange.c_str());
				pXSpec->getChangelog().lastDate().addEntry(change);
			}
			sChange.assign("");
		}
		sChange.append(szIn, nLen);
		szIn = szOut;
	}
	if (sChange.length()) {
		XMLChangelogEntry change(sChange.c_str());
		pXSpec->getChangelog().lastDate().addEntry(change);
	}
	return true;
}

XMLChangelogEntry::XMLChangelogEntry(const char* szChange)
	: XMLBase()
{
	m_sChange.assign(szChange);
}

XMLChangelogEntry::XMLChangelogEntry(const XMLChangelogEntry& rEntry)
	: XMLBase()
{
	m_sChange.assign(rEntry.m_sChange);
}

XMLChangelogEntry::~XMLChangelogEntry()
{
}

void XMLChangelogEntry::toSpecFile(ostream& rOut)
{
	rOut << endl << "- " << getChange();
}

void XMLChangelogEntry::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t\t\t<change>" << getChange() << "</change>";
}

// attribute structure for XMLChangelogDate
structValidAttrs g_paChangelogDateAttrs[] =
{
	{0x0000,    true,  false, "date",         XATTRTYPE_DATE,   {NULL}},
	{0x0001,    true,  false, "author",       XATTRTYPE_STRING, {"*", NULL}},
	{0x0002,    false, false, "author-email", XATTRTYPE_MAIL,   {NULL}},
	{0x0003,    false, false, "version",      XATTRTYPE_STRING, {"*", NULL}},
	{XATTR_END, false, false, "end",          XATTRTYPE_NONE,   {NULL}}
};

bool XMLChangelogDate::parseCreate(XMLAttrs* pAttrs,
								   XMLSpec* pSpec)
{
	// validate our attributes
	if (!pAttrs->validate(g_paChangelogDateAttrs, (XMLBase*)pSpec))
		return false;

	XMLChangelogDate date(pAttrs->asString("date"),
						  pAttrs->asString("author"),
						  pAttrs->asString("author-email"),
						  pAttrs->asString("version"));
	pSpec->getChangelog().addDate(date);
	return true;
}

bool XMLChangelogDate::structCreate(const char* szDate,
									const char* szName,
									const char* szEntries,
									XMLSpec* pXSpec)
{
	if (!szDate || !szName || !szEntries || ! pXSpec)
		return false;
	time_t tTime = (time_t)(atol(szDate)) - timezone;
	struct tm *sTime = gmtime(&tTime);
	sTime->tm_year += 1900;
	char szTmp[32];
	sprintf(szTmp,"%s %s %d %d", g_szaDays[sTime->tm_wday],
								  g_szaMonths[sTime->tm_mon],
								  sTime->tm_mday, sTime->tm_year);
	XMLChangelogDate date(szTmp, szName, NULL, NULL);
	pXSpec->getChangelog().addDate(date);
	XMLChangelogEntry::structCreate(szEntries, pXSpec);
	return true;
}

XMLChangelogDate::XMLChangelogDate(const char* szDate,
								   const char* szAuthor,
								   const char* szEmail,
								   const char* szVersion)
	: XMLBase()
{
	if (szDate)
		m_sDate.assign(szDate);
	if (szAuthor)
		m_sAuthor.assign(szAuthor);
	if (szEmail)
		m_sEmail.assign(szEmail);
	if (szVersion)
		m_sVersion.assign(szVersion);
}

XMLChangelogDate::XMLChangelogDate(const XMLChangelogDate& rDate)
	: XMLBase()
{
	m_sDate.assign(rDate.m_sDate);
	m_sAuthor.assign(rDate.m_sAuthor);
	m_sEmail.assign(rDate.m_sEmail);
	m_sVersion.assign(rDate.m_sVersion);
	m_vEntries = rDate.m_vEntries;
}

XMLChangelogDate::~XMLChangelogDate()
{
}

void XMLChangelogDate::toSpecFile(ostream& rOut)
{
	rOut << endl << "* " << getDate() << " " << getAuthor();
	if (hasEmail())
		rOut << " <" << getEmail() << ">";
	if (hasVersion())
		rOut << " " << getVersion();
	for (unsigned int i = 0; i < numEntries(); i++)
		getEntry(i).toSpecFile(rOut);
	rOut << endl;
}

void XMLChangelogDate::toXMLFile(ostream& rOut)
{
	rOut << endl << "\t\t<changes date=\"" << getDate() << "\"";
	rOut << endl << "\t\t         author=\"" << getAuthor() << "\"";
	if (hasEmail())
		rOut << endl << "\t\t         author-email=\"" << getEmail() << "\"";
	if (hasVersion())
		rOut << endl << "\t\t         version=\"" << getVersion() << "\"";
	rOut << ">";
	for (unsigned int i = 0; i < numEntries(); i++)
		getEntry(i).toXMLFile(rOut);
	rOut << endl << "\t\t</changes>";
}

bool XMLChangelog::structCreate(Spec pSpec,
								XMLSpec* pXSpec)
{
	if (!pXSpec || !pSpec || !pSpec->packages || !pSpec->packages->header)
		return false;
	// FIXME: it looks like RPM only stores the tomost date in the
	// spec file so we are only allowed to get that one instead of an
	// array of time_t's
	string sDates;
	t_StrVector svChanges;
	t_StrVector svNames;
	getRPMHeader(pSpec->packages->header, RPMTAG_CHANGELOGTIME, sDates);
	getRPMHeaderArray(pSpec->packages->header, RPMTAG_CHANGELOGNAME, svNames);
	getRPMHeaderArray(pSpec->packages->header, RPMTAG_CHANGELOGTEXT, svChanges);
	for (unsigned int i = 0; i < svNames.size(); i++)
		XMLChangelogDate::structCreate(sDates.c_str(), svNames[i].c_str(),
									   svChanges[i].c_str(), pXSpec);
	return true;
}

XMLChangelog::XMLChangelog()
	: XMLBase()
{
}

XMLChangelog::XMLChangelog(const XMLChangelog& rChangelog)
	: XMLBase()
{
	m_vDates = rChangelog.m_vDates;
}

XMLChangelog::~XMLChangelog()
{
}

void XMLChangelog::toSpecFile(ostream& rOut)
{
	if (numDates()) {
		rOut << endl << "%changelog";
		for (unsigned int i = 0; i < numDates(); i++)
			getDate(i).toSpecFile(rOut);
		rOut << endl;
	}
}

void XMLChangelog::toXMLFile(ostream& rOut)
{
	if (numDates()) {
		rOut << endl << "\t<changelog>";
		for (unsigned int i = 0; i < numDates(); i++)
			getDate(i).toXMLFile(rOut);
		rOut << endl << "\t</changelog>";
	}
}

void XMLChangelog::toRPMStruct(Spec spec)
{
}
