/** \ingroup signature
 * \file rpmio/base64.c
 *
 */

static int _debug = 0;

#include "system.h"

#include "base64.h"

int B64decode (const char * s, void ** datap, size_t *lenp)
{
    static char * b64dec = NULL;
    unsigned char *t, *te;
    size_t ns, nt;
    int a, b, c, d;

    if (s == NULL)	return 1;
    ns = strlen(s);
    if (ns & 0x3)	return 2;

    if (b64dec == NULL) {
	b64dec = xmalloc(255);
	memset(b64dec, 0x80, 255);
	for (c = 'A'; c <= 'Z'; c++)
	    b64dec[ c ] = 0 + (c - 'A');
	for (c = 'a'; c <= 'z'; c++)
	    b64dec[ c ] = 26 + (c - 'a');
	for (c = '0'; c <= '9'; c++)
	    b64dec[ c ] = 52 + (c - '0');
	b64dec['+'] = 62;
	b64dec['/'] = 63;
	b64dec['='] = 0;
    }
    
    nt = (ns / 4) * 3;
    t = te = xmalloc(nt + 1);

    while (ns > 0) {
	if ((a = b64dec[ *s++ ]) == 0x80)
	    break;
	if ((b = b64dec[ *s++ ]) == 0x80)
	    break;
	if ((c = b64dec[ *s++ ]) == 0x80)
	    break;
	if ((d = b64dec[ *s++ ]) == 0x80)
	    break;
if (_debug)
fprintf(stderr, "%7u %02x %02x %02x %02x -> %02x %02x %02x\n",
(unsigned)ns, a, b, c, d,
(((a << 2) | (b >> 4)) & 0xff),
(((b << 4) | (c >> 2)) & 0xff),
(((c << 6) | d) & 0xff));
	ns -= 4;
	*te++ = (a << 2) | (b >> 4);
	if (s[-2] == '=') break;
	*te++ = (b << 4) | (c >> 2);
	if (s[-1] == '=') break;
	*te++ = (c << 6) | d;
    }

    if (ns > 0) {
	free(t);
	return 3;
    }
    if (lenp)
	*lenp = (te - t);
    if (datap)
	*datap = t;
    else
	free(t);

    return 0;
}

char * B64encode (const void * str, size_t ns)
{
    static char b64enc[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *s = str;
    unsigned char *t, *te;
    size_t nt;
    unsigned c;

    if (s == NULL)	return NULL;
    if (*s == '\0')	return xstrdup("");

    if (ns == 0) ns = strlen(s);
    nt = ((ns + 2) / 3) * 4;
    t = te = xmalloc(nt + 1);

    while (ns) {

if (_debug)
fprintf(stderr, "%7u %02x %02x %02x -> %02x %02x %02x %02x\n",
(unsigned)ns, s[0], s[1], s[2],
(s[0] >> 2),
((s[0] & 0x3) << 4) | (s[1] >> 4),
((s[1] & 0xf) << 2) | (s[2] >> 6),
(s[2]& 0x3f));
	c = *s++;
	*te++ = b64enc[ (c >> 2) ];
	*te++ = b64enc[ ((c & 0x3) << 4) | (*s >> 4) ];
	if (--ns <= 0) {
	    *te++ = '=';
	    *te++ = '=';
	    continue;
	}
	c = *s++;
	*te++ = b64enc[ ((c & 0xf) << 2) | (*s >> 6) ];
	if (--ns <= 0) {
	    *te++ = '=';
	    continue;
	}
	*te++ = b64enc[ (*s & 0x3f) ];
	s++;
	--ns;
    }
    *te = '\0';
    return t;
}
