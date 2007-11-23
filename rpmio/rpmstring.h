#ifndef _RPMSTRING_H_
#define _RPMSTRING_H_

/** \ingroup rpmstring
 * \file rpmio/rpmstring.h
 * String manipulation helper functions
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmstring
 * Locale insensitive islower(3) 
 */
static inline int xislower(int c)  {
    return (c >= 'a' && c <= 'z');
}

/** \ingroup rpmstring
 * Locale insensitive isupper(3)
 */
static inline int xisupper(int c)  {
    return (c >= 'A' && c <= 'Z');
}

/** \ingroup rpmstring
 * Locale insensitive isalpha(3)
 */
static inline int xisalpha(int c)  {
    return (xislower(c) || xisupper(c));
}

/** \ingroup rpmstring
 * Locale insensitive isdigit(3)
 */
static inline int xisdigit(int c)  {
    return (c >= '0' && c <= '9');
}

/** \ingroup rpmstring
 * Locale insensitive isalnum(3)
 */
static inline int xisalnum(int c)  {
    return (xisalpha(c) || xisdigit(c));
}

/** \ingroup rpmstring
 * Locale insensitive isblank(3)
 */
static inline int xisblank(int c)  {
    return (c == ' ' || c == '\t');
}

/** \ingroup rpmstring
 * Locale insensitive isspace(3)
 */
static inline int xisspace(int c)  {
    return (xisblank(c) || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

/** \ingroup rpmstring
 * Locale insensitive tolower(3)
 */
static inline int xtolower(int c)  {
    return ((xisupper(c)) ? (c | ('a' - 'A')) : c);
}

/** \ingroup rpmstring
 * Locale insensitive toupper(3)
 */
static inline int xtoupper(int c)  {
    return ((xislower(c)) ? (c & ~('a' - 'A')) : c);
}

/** \ingroup rpmstring
 * Locale insensitive strcasecmp(3).
 */
int xstrcasecmp(const char * s1, const char * s2)		;

/** \ingroup rpmstring
 * Locale insensitive strncasecmp(3).
 */
int xstrncasecmp(const char *s1, const char * s2, size_t n)	;

/** \ingroup rpmstring
 * Split string into fields separated by a character.
 * @param str		string
 * @param length	length of string
 * @param sep		separator character
 * @return		(malloc'd) argv array
 */
char ** splitString(const char * str, int length, char sep);

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
