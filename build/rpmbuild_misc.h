#ifndef _RPMBUILD_MISC_H
#define _RPMBUILD_MISC_H

#include <sys/types.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmds.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmbuild
 * Truncate comment lines.
 * @param s		skip white space, truncate line at '#'
 */
RPM_GNUC_INTERNAL
void handleComments(char * s);

/** \ingroup rpmstring
 */
typedef struct StringBufRec *StringBuf;

/** \ingroup rpmstring
 */
RPM_GNUC_INTERNAL
StringBuf newStringBuf(void);

/** \ingroup rpmstring
 */
RPM_GNUC_INTERNAL
StringBuf freeStringBuf( StringBuf sb);

/** \ingroup rpmstring
 */
RPM_GNUC_INTERNAL
const char * getStringBuf(StringBuf sb);

/** \ingroup rpmstring
 */
RPM_GNUC_INTERNAL
void stripTrailingBlanksStringBuf(StringBuf sb);

/** \ingroup rpmstring
 */
#define appendStringBuf(sb, s)     appendStringBufAux(sb, s, 0)

/** \ingroup rpmstring
 */
#define appendLineStringBuf(sb, s) appendStringBufAux(sb, s, 1)

/** \ingroup rpmstring
 */
RPM_GNUC_INTERNAL
void appendStringBufAux(StringBuf sb, const char * s, int nl);

/** \ingroup rpmbuild
 * Parse an unsigned number.
 * @param		line from spec file
 * @retval res		pointer to uint32_t
 * @return		0 on success, 1 on failure
 */
RPM_GNUC_INTERNAL
uint32_t parseUnsignedNum(const char * line, uint32_t * res);

/** \ingroup rpmbuild
 * Add dependency to header, filtering duplicates.
 * @param h		header
 * @param tagN		tag, identifies type of dependency
 * @param N		(e.g. Requires: foo < 0:1.2-3, "foo")
 * @param EVR		(e.g. Requires: foo < 0:1.2-3, "0:1.2-3")
 * @param Flags		(e.g. Requires: foo < 0:1.2-3, both "Requires:" and "<")
 * @param index		(0 always)
 * @return		0 on success, 1 on error
 */
RPM_GNUC_INTERNAL
int addReqProv(Header h, rpmTagVal tagN,
		const char * N, const char * EVR, rpmsenseFlags Flags,
		uint32_t index);

/** \ingroup rpmbuild
 * Add rpmlib feature dependency.
 * @param h		header
 * @param feature	rpm feature name (i.e. "rpmlib(Foo)" for feature Foo)
 * @param featureEVR	rpm feature epoch/version/release
 * @return		0 always
 */
RPM_GNUC_INTERNAL
int rpmlibNeedsFeature(Header h, const char * feature, const char * featureEVR);

#ifdef __cplusplus
}
#endif

#endif /* _RPMBUILD_MISC_H */
