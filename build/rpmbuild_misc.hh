#ifndef _RPMBUILD_MISC_H
#define _RPMBUILD_MISC_H

#include <string>

#include <sys/types.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmds.h>

/** \ingroup rpmbuild
 * Truncate comment lines.
 * @param s		skip white space, truncate line at '#'
 * @return		1 on comment lines, 0 otherwise
 */
int handleComments(char * s);

struct Source *findSource(rpmSpec spec, uint32_t num, int flag);

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
const char * getStringBuf(StringBuf sb);

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
void appendStringBufAux(StringBuf sb, const std::string & s, int nl);

/** \ingroup rpmbuild
 * Parse an unsigned number.
 * @param		line from spec file
 * @param[out] res		pointer to uint32_t
 * @return		0 on success, -1 on failure
 */
int parseUnsignedNum(const char * line, uint32_t * res);

/* pointer distance for strlen() compatibility, pe >= p known */
inline size_t ptrlen(const char *p, const char *pe)
{
    return pe-p;
}
#endif /* _RPMBUILD_MISC_H */
