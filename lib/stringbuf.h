#ifndef _STRINGBUF_H_
#define _STRINGBUF_H_

/** \file lib/stringbuf.h
 *
 */

/**
 */
typedef /*@abstract@*/ struct StringBufRec *StringBuf;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
/*@only@*/ StringBuf newStringBuf(void)
	/*@*/;

/**
 */
/*@null@*/ StringBuf freeStringBuf( /*@only@*/ /*@null@*/ StringBuf sb)
	/*@modifies sb @*/;

/**
 */
/*@unused@*/
void truncStringBuf(StringBuf sb)
	/*@modifies sb @*/;

/**
 */
/*@observer@*/ char * getStringBuf(StringBuf sb)
	/*@*/;

/**
 */
void stripTrailingBlanksStringBuf(StringBuf sb)
	/*@modifies sb @*/;

/**
 */
#define appendStringBuf(sb, s)     appendStringBufAux(sb, s, 0)

/**
 */
#define appendLineStringBuf(sb, s) appendStringBufAux(sb, s, 1)

/**
 */
void appendStringBufAux(StringBuf sb, const char * s, int nl)
	/*@modifies sb @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _STRINGBUF_H_ */
