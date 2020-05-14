#ifndef _RPMVER_H
#define _RPMVER_H

/** \ingroup rpmtrans
 * Segmented string compare for version or release strings.
 *
 * @param a		1st string
 * @param b		2nd string
 * @return		+1 if a is "newer", 0 if equal, -1 if b is "newer"
 */
int rpmvercmp(const char * a, const char * b);

#endif /* _RPMVER_H */
