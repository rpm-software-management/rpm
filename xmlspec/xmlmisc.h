#ifndef _XML_MISC_H_
#define _XML_MISC_H_

#include "stringbuf.h"

void freeStr(char** pszStr);
char* newStr(const char* szValue,
	     char** pszStr);
char* newStrEx(const char* szValue,
	       char** pszStr);

void addMacroEx(char* szName,
		char** pszVar,
		int nLevel);

int strToBool(const char* szBool);

StringBuf fileToStrBuf(const char* szFile,
		       const char* szPrepend);
char* fileToStr(const char* szFile,
		const char* szPrepend);

#endif
