/** \ingroup rpmbuild
 * \file build/misc.c
 */
#include "system.h"

#include <string>
#include <algorithm>

#include <ctype.h>
#include <stdlib.h>
#include <rpm/rpmstring.h>
#include "rpmbuild_misc.h"
#include "debug.h"

#define BUF_CHUNK 1024

struct StringBufRec {
    std::string buf;
};

StringBuf newStringBuf(void)
{
    return new StringBufRec {};
}

StringBuf freeStringBuf(StringBuf sb)
{
    delete sb;
    return NULL;
}

void stripTrailingBlanksStringBuf(StringBuf sb)
{
    while (!sb->buf.empty() && risspace(sb->buf.back()))
	sb->buf.erase(sb->buf.size() - 1);
}

const char * getStringBuf(StringBuf sb)
{
    return (sb != NULL) ? sb->buf.c_str() : NULL;
}

void appendStringBufAux(StringBuf sb, const std::string & s, int nl)
{
    sb->buf += s;
    if (nl)
	sb->buf += '\n';
}

int parseUnsignedNum(const char * line, uint32_t * res)
{
    char * s1 = NULL;
    unsigned long rc;
    uint32_t result;

    if (line == NULL) return -1;

    while (isspace(*line)) line++;
    if (!isdigit(*line)) return -1;

    rc = strtoul(line, &s1, 10);

    if (*s1 || s1 == line || rc == ULONG_MAX || rc > UINT_MAX)
        return -1;

    result = (uint32_t)rc;
    if (res) *res = result;

    return 0;
}
