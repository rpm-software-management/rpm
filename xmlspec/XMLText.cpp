// our includes
#include "XMLText.h"

using namespace std;

XMLText::XMLText(const char* szText,
				 const char* szLang)
	: XMLBase()
{
	setText(szText);
	setLang(szLang);
}

XMLText::XMLText(const XMLText& rText)
	: XMLBase()
{
	setText(rText.m_sText.c_str());
	setLang(rText.m_sLang.c_str());
}

XMLText::~XMLText()
{
}
