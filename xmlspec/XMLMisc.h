#ifndef _H_XMLMISC_
#define _H_XMLMISC_

extern char* g_szaDays[];
extern char* g_szaMonths[];

/**
 * Splits the input string according to the terminator, returning
 * the length and the new search position
 * .
 * @param szInput The input string
 * @param cTerm The terminating character
 * @param nLen the length buffer
 * @return The next string position
 **/
extern char* splitStr(const char* szInput,
					  char cTerm,
					  int& nLen);

/**
 * Finds a string in an array of potential matches
 * .
 * @param szaMatches The potential matches
 * @param szValue The value to search for
 * @return The position on success, -1 if not found
 **/
extern int findStr(char* szaMatches[],
				   const char* szValue,
				   int nLen = -1);

/**
 * Checks if a string contains an integer
 * .
 * @param szValue The string to check
 * @param nLen The length to check, -1 to end of string
 * @return true if the string is an integer, false otherwise
 **/
extern bool isInteger(const char* szValue,
					  int nLen = -1);

/**
 * Checks if a string contains a boolean value
 * .
 * @param szValue The value to check
 * @return true if we have a boolean, false otherwise
 **/
extern bool isBool(const char* szValue,
				   int nLen = -1);

/**
 * Checks if a string is in a valid date format
 * .
 * @param szValue The string to check
 * @return true is this is a date, false otherwise
 **/
extern bool isDate(const char* szValue);

/**
 * Checks if a string contains a valid e-mail address
 * .
 * @param szValue the string to check
 * @return true if this is an email address, false otherwise
 **/
extern bool isEmail(const char* szValue);

#endif
