#ifndef _RPMSTRING_H_
#define _RPMSTRING_H_

/** \file rpmio/rpmstring.h
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

static inline int xislower(int c)  {
    return (c >= 'a' && c <= 'z');
}
static inline int xisupper(int c)  {
    return (c >= 'A' && c <= 'Z');
}
static inline int xisalpha(int c)  {
    return (xislower(c) || xisupper(c));
}
static inline int xisdigit(int c)  {
    return (c >= '0' && c <= '9');
}
static inline int xisalnum(int c)  {
    return (xisalpha(c) || xisdigit(c));
}
static inline int xisblank(int c)  {
    return (c == ' ' || c == '\t');
}
static inline int xisspace(int c)  {
    return (xisblank(c) || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static inline int xtolower(int c)  {
    return ((xisupper(c)) ? (c | ('a' - 'A')) : c);
}
static inline int xtoupper(int c)  {
    return ((xislower(c)) ? (c & ~('a' - 'A')) : c);
}

/** \ingroup rpmio
 * Locale insensitive strcasecmp(3).
 */
int xstrcasecmp(const char * s1, const char * s2)		;

/** \ingroup rpmio
 * Locale insensitive strncasecmp(3).
 */
int xstrncasecmp(const char *s1, const char * s2, size_t n)	;

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
