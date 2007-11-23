#ifndef _RPMSTRING_H_
#define _RPMSTRING_H_

/** \file rpmio/rpmstring.h
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Split string into fields separated by a character.
 * @param str		string
 * @param length	length of string
 * @param sep		separator character
 * @return		(malloc'd) argv array
 */
char ** splitString(const char * str, int length, char sep);

/**
 * Free split string argv array.
 * @param list		argv array
 */
void freeSplitString( char ** list);

/**
 * Remove occurences of trailing character from string.
 * @param s		string
 * @param c		character to strip
 * @return 		string
 */
char * stripTrailingChar(char * s, char c);

/**
 */
typedef struct StringBufRec *StringBuf;

/**
 */
StringBuf newStringBuf(void);

/**
 */
StringBuf freeStringBuf( StringBuf sb);

/**
 */
void truncStringBuf(StringBuf sb);

/**
 */
char * getStringBuf(StringBuf sb);

/**
 */
void stripTrailingBlanksStringBuf(StringBuf sb);

/**
 */
#define appendStringBuf(sb, s)     appendStringBufAux(sb, s, 0)

/**
 */
#define appendLineStringBuf(sb, s) appendStringBufAux(sb, s, 1)

/**
 */
void appendStringBufAux(StringBuf sb, const char * s, int nl);

#ifdef __cplusplus
}
#endif

#endif	/* _RPMSTRING_H_ */
