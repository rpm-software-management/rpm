#ifndef _RPMSTRING_H_
#define _RPMSTRING_H_

/** \ingroup rpmstring
 * \file rpmio/rpmstring.h
 * String manipulation helper functions
 */

#include <stddef.h>

#include <rpm/rpmutil.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmstring
 * Locale insensitive islower(3) 
 */
static inline int rislower(int c)  {
    return (c >= 'a' && c <= 'z');
}

/** \ingroup rpmstring
 * Locale insensitive isupper(3)
 */
static inline int risupper(int c)  {
    return (c >= 'A' && c <= 'Z');
}

/** \ingroup rpmstring
 * Locale insensitive isalpha(3)
 */
static inline int risalpha(int c)  {
    return (rislower(c) || risupper(c));
}

/** \ingroup rpmstring
 * Locale insensitive isdigit(3)
 */
static inline int risdigit(int c)  {
    return (c >= '0' && c <= '9');
}

/** \ingroup rpmstring
 * Locale insensitive isalnum(3)
 */
static inline int risalnum(int c)  {
    return (risalpha(c) || risdigit(c));
}

/** \ingroup rpmstring
 * Locale insensitive isblank(3)
 */
static inline int risblank(int c)  {
    return (c == ' ' || c == '\t');
}

/** \ingroup rpmstring
 * Locale insensitive isspace(3)
 */
static inline int risspace(int c)  {
    return (risblank(c) || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

/** \ingroup rpmstring
 * Locale insensitive tolower(3)
 */
static inline int rtolower(int c)  {
    return ((risupper(c)) ? (c | ('a' - 'A')) : c);
}

/** \ingroup rpmstring
 * Locale insensitive toupper(3)
 */
static inline int rtoupper(int c)  {
    return ((rislower(c)) ? (c & ~('a' - 'A')) : c);
}

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static inline unsigned char rnibble(char c)
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
	return (c - 'a') + 10;
    return 0;
}

/** \ingroup rpmstring
 * Locale insensitive strcasecmp(3).
 */
int rstrcasecmp(const char * s1, const char * s2)		;

/** \ingroup rpmstring
 * Locale insensitive strncasecmp(3).
 */
int rstrncasecmp(const char *s1, const char * s2, size_t n)	;

/** \ingroup rpmstring
 * asprintf() clone
 */
int rasprintf(char **strp, const char *fmt, ...) RPM_GNUC_PRINTF(2, 3);

/** \ingroup rpmstring
 * Concatenate strings with dynamically (re)allocated memory.
 * @param dest		pointer to destination string
 * @param src		source string
 * @return		realloc'd dest with src appended
 */
char *rstrcat(char **dest, const char *src);

/** \ingroup rpmstring
 * Split string into fields separated by a character.
 * @param str		string
 * @param length	length of string
 * @param sep		separator character
 * @return		(malloc'd) argv array
 */
char ** splitString(const char * str, size_t length, char sep);

/** \ingroup rpmstring
 * Free split string argv array.
 * @param list		argv array
 */
void freeSplitString( char ** list);

/** \ingroup rpmstring
 * Remove occurences of trailing character from string.
 * @param s		string
 * @param c		character to strip
 * @return 		string
 */
char * stripTrailingChar(char * s, char c);

/** \ingroup rpmstring
 */
typedef struct StringBufRec *StringBuf;

/** \ingroup rpmstring
 */
StringBuf newStringBuf(void);

/** \ingroup rpmstring
 */
StringBuf freeStringBuf( StringBuf sb);

/** \ingroup rpmstring
 */
void truncStringBuf(StringBuf sb);

/** \ingroup rpmstring
 */
char * getStringBuf(StringBuf sb);

/** \ingroup rpmstring
 */
void stripTrailingBlanksStringBuf(StringBuf sb);

/** \ingroup rpmstring
 */
#define appendStringBuf(sb, s)     appendStringBufAux(sb, s, 0)

/** \ingroup rpmstring
 */
#define appendLineStringBuf(sb, s) appendStringBufAux(sb, s, 1)

/** \ingroup rpmstring
 */
void appendStringBufAux(StringBuf sb, const char * s, int nl);

#ifdef __cplusplus
}
#endif

#endif	/* _RPMSTRING_H_ */
