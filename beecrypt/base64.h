/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file base64.h
 * \brief Base64 encoding and decoding, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#ifndef _BASE64_H
#define _BASE64_H

#include "beecrypt.h"

/*!\
 * Decode white space character set (default).
 */
extern const char* b64decode_whitespace;
#define B64DECODE_WHITESPACE	" \f\n\r\t\v"

/*!\
 * Encode 72 characters per line (default).
 */
extern int b64encode_chars_per_line;
#define B64ENCODE_CHARS_PER_LINE	72

/*!\
 * Encode end-of-line string (default).
 */
extern const char* b64encode_eolstr;
#define B64ENCODE_EOLSTR	"\n"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Encode chunks of 3 bytes of binary input into 4 bytes of base64 output.
 * \param data binary data
 * \param ns no. bytes of data (0 uses strlen(data))
 * \return (malloc'd) base64 string
 */
BEECRYPTAPI
char* b64encode(const void* data, size_t ns);

/*!
 * Encode crc of binary input data into 5 bytes of base64 output.
 * \param data binary data
 * \param ns no. bytes of binary data
 * \return (malloc'd) base64 string
 */
BEECRYPTAPI
char* b64crc(const unsigned char* data, size_t ns);

/*!
 * Decode chunks of 4 bytes of base64 input into 3 bytes of binary output.
 * \param s base64 string
 * \retval datap address of (malloc'd) binary data
 * \retval lenp	 address of no. bytes of binary data
 * \return 0 on success, 1: s == NULL, 2: bad length, 3: bad char
 */
BEECRYPTAPI
int b64decode(const char* s, void** datap, size_t* lenp);

/*!
 */
BEECRYPTAPI
char*		b64enc(const memchunk*);

/*!
 */
BEECRYPTAPI
memchunk*	b64dec(const char*);

#ifdef __cplusplus
}
#endif

#endif
