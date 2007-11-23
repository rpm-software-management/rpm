#ifndef _RPMSTRING_H_
#define _RPMSTRING_H_

/** \file rpmio/rpmstring.h
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

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
