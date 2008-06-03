#ifndef H_MISC
#define H_MISC

/**
 * \file lib/misc.h
 *
 */

#include <string.h>
#include <rpm/rpmtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create directory if it does not exist, and make sure path is writable.
 * @note This will only create last component of directory path.
 * @param dpath		directory path
 * @param dname		directory use string
 * @return		rpmRC return code
 */
rpmRC rpmMkdirPath (const char * dpath, const char * dname);

#ifdef __cplusplus
}
#endif

#endif	/* H_MISC */
