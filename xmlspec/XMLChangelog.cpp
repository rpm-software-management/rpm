// standard C++ includes
#include <string>

// our includes
#include "XMLChangelog.h"
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
	{0x0000,    true,  false, "date"},
	{0x0001,    true,  false, "author"},
	{0x0002,    false, false, "author-email"},
	{0x0003,    false, false, "version"},
	{XATTR_END, false, false, "end"}
};

bool XMLChangelogDate::parseCreate(XMLAttrs* pAttrs,
								   XMLSpec* pSpec)
{
	// validate our attributes
	if (!pAttrs->validate(g_paChangelogDateAttrs, (XMLBase*)pSpec))
		return false;

	XMLChangelogDate date(pAttrs->get("date"),
						  pAttrs->get("author"),
						  pAttrs->get("author-email"),
						  pAttrs->get("version"));
	pSpec->getChangelog().addDate(date);
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
	if (numEntries()) {
		rOut << endl << "* " << getDate() << " " << getAuthor();
		if (hasEmail())
			rOut << " <" << getEmail() << ">";
		if (hasVersion())
			rOut << " " << getVersion();
		for (unsigned int i = 0; i < numEntries(); i++)
			getEntry(i).toSpecFile(rOut);
		rOut << endl;
	}
}

void XMLChangelogDate::toXMLFile(ostream& rOut)
{
	if (numEntries()) {
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
		rOut << endl << "\t<changelog>";
	}
}

void XMLChangelog::toRPMStruct(Spec spec)
{
}
