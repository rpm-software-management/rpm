#ifndef H_BASE64
#define H_BASE64

/** \ingroup rpmio
 * \file rpmio/base64.h
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Decode chunks of 4 bytes of base64 input into 3 bytes of binary output.
 * @param s		base64 string
 * @retval datap	address of (malloc'd) binary data
 * @retval lenp		address of no. bytes of binary data
 * @return		0 on success
 */
/*@unused@*/
int b64decode (const char * s, /*@out@*/ void ** datap, /*@out@*/ size_t *lenp);

/**
 * Encode chunks of 3 bytes of binary input into 4 bytes of base64 output.
 * @param data		binary data
 * @param ns		no. bytes of data (0 uses strlen(data))
 * @return		(malloc'd) base64 string
 */
/*@unused@*/
char * b64encode (const void * data, size_t ns);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMURL */
