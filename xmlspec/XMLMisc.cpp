// standard c includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

char* g_szaDays[]   = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL };
char* g_szaMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
int g_naLengths[]   = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
char* g_szaBools[]  = { "no", "yes", "false", "true", "0", "1", NULL };

char* splitStr(const char* szInput,
			   char cTerm,
			   int& nLen)
{
	nLen = 0;
	while (szInput[nLen] != cTerm && szInput[nLen] != '\0') {
		nLen++;
	}
	char* szTemp = ((char*)szInput)+nLen;
	while (*szTemp == cTerm && *szTemp != '\0')
		szTemp++;
	return szTemp;
}

int findStr(char* szaMatches[],
			const char* szValue,
			int nLen = -1)
{
	for (unsigned int i = 0; szaMatches[i] != NULL; i++) {
		if (strcmp(szaMatches[i], "*") == 0)
			return i;
		else if (nLen == -1) {
			if (strcasecmp(szaMatches[i], szValue) == 0)
				return i;
		}
		else if (strncasecmp(szaMatches[i], szValue, nLen) == 0)
			return i;
	}
	return -1;
}

bool isInteger(const char* szValue,
			   int nLen = -1)
{
	if (nLen == -1)
		nLen = strlen(szValue);
	for (unsigned int i = 0; i < strlen(szValue); i++) {
		if (szValue[i] < '0' || szValue[i] > '9')
			return false;
	}
	return true;
}

bool isBool(const char* szValue,
			int nLen = -1)
{
	return findStr(g_szaBools, szValue, nLen) != -1 ? true : false;
}

bool isDate(const char* szValue)
{
	int nLen, nPos;
	char* szTemp = splitStr(szValue, ' ', nLen);
	if ((nPos = findStr(g_szaDays, szValue, nLen)) != -1) {
		if ((nPos = findStr(g_szaMonths, szTemp, nLen)) != -1) {
			szTemp = splitStr(szTemp, ' ', nLen);
			char* szBuffer = new char[nLen+1];
			sprintf(szBuffer, "%s", szTemp);
			szBuffer[nLen] = '\0';
			if (atoi(szBuffer) <= g_naLengths[nPos]) {
				delete[] szBuffer;
				szTemp = splitStr(szTemp, ' ', nLen);
				return isInteger(szTemp, nLen);
			}
			delete[] szBuffer;
		}
	}
	return false;
}

bool isEmail(const char* szValue)
{
	bool bFound = false;
	for (unsigned int j = 0; j < strlen(szValue); j++) {
		if (szValue[j] == '@') {
			if (bFound)
				return false;
			else
				bFound = true;
		}
	}
	return bFound;
}
