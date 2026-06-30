#ifndef _RPMCHROOT_H
#define _RPMCHROOT_H

#include <rpm/rpmutil.h>

/** \ingroup rpmchroot
 * Set or clear process-wide chroot directory.
 * Calling this while chrooted is an error.
 * param rootDir	new chroot directory (or NULL to reset)
 * return		-1 on error, 0 on success
 */
int rpmChrootSet(const char *rootDir);

/** \ingroup rpmchroot
 * Return absolute path to current chroot directory.
 * return		chroot directory (or NULL if "/" or unset)
 */
const char *rpmChrootPath(void);

/** \ingroup rpmchroot
 * Enter chroot if necessary.
 * return		-1 on error, 0 on success.
 */
int rpmChrootIn(void);

/** \ingroup rpmchroot
 * Return from chroot if necessary.
 * return		-1 on error, 0 succes.
 */
int rpmChrootOut(void);

/** \ingroup rpmchroot
 * Return chrooted status.
 * return		1 if chrooted, 0 otherwise
 */
int rpmChrootDone(void);

#endif /* _RPMCHROOT_H */
