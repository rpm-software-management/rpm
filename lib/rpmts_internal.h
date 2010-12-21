#ifndef _RPMTS_INTERNAL_H
#define _RPMTS_INTERNAL_H

#include <rpm/rpmts.h>

#include "lib/rpmal.h"		/* XXX availablePackage */
#include "lib/fprint.h"
#include "lib/rpmlock.h"

typedef struct diskspaceInfo_s * rpmDiskSpaceInfo;

/* Transaction set elements information */
typedef struct tsMembers_s {
    intHash removedPackages;	/*!< Set of packages being removed. */
    rpmal addedPackages;	/*!< Set of packages being installed. */

    rpmte * order;		/*!< Packages sorted by dependencies. */
    int orderCount;		/*!< No. of transaction elements. */
    int orderAlloced;		/*!< No. of allocated transaction elements. */
    int delta;			/*!< Delta for reallocation. */
} * tsMembers;

/** \ingroup rpmts
 * The set of packages to be installed/removed atomically.
 */
struct rpmts_s {
    rpmtransFlags transFlags;	/*!< Bit(s) to control operation. */

    int (*solve) (rpmts ts, rpmds key, const void * data);
                                /*!< Search for NEVRA key. */
    const void * solveData;	/*!< Solve callback data */

    rpmCallbackFunction notify;	/*!< Callback function. */
    rpmCallbackData notifyData;	/*!< Callback private data. */

    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    rpmDiskSpaceInfo dsi;	/*!< Per filesystem disk/inode usage. */

    rpmdb rdb;			/*!< Install database handle. */
    int dbmode;			/*!< Install database open mode. */

    tsMembers members;		/*!< Transaction set member info (order etc) */

    struct selabel_handle * selabelHandle;	/*!< Handle to selabel */

    char * rootDir;		/*!< Path to top of install tree. */
    char * lockPath;		/*!< Transaction lock path */
    FD_t scriptFd;		/*!< Scriptlet stdout/stderr. */
    rpm_tid_t tid;		/*!< Transaction id. */

    rpm_color_t color;		/*!< Transaction color bits. */
    rpm_color_t prefcolor;	/*!< Preferred file color. */

    rpmVSFlags vsflags;		/*!< Signature/digest verification flags. */

    rpmKeyring keyring;		/*!< Keyring in use. */

    ARGV_t netsharedPaths;	/*!< From %{_netsharedpath} */
    ARGV_t installLangs;	/*!< From %{_install_langs} */

    struct rpmop_s ops[RPMTS_OP_MAX];

    rpmPlugins plugins;		/*!< Transaction plugins */

    int nrefs;			/*!< Reference count. */
};

#ifdef __cplusplus
extern "C" {
#endif
 
RPM_GNUC_INTERNAL
tsMembers rpmtsMembers(rpmts ts);

/* returns -1 for retry, 0 for ignore and 1 for not found */
RPM_GNUC_INTERNAL
int rpmtsSolve(rpmts ts, rpmds key);

RPM_GNUC_INTERNAL
rpmlock rpmtsAcquireLock(rpmts ts);

/** \ingroup rpmts
 * Get the selabel handle from the transaction set
 * @param ts		transaction set
 * @return		rpm selabel handle, or NULL if it hasn't been initialized yet
 */
struct selabel_handle * rpmtsSELabelHandle(rpmts ts);

/** \ingroup rpmts
 * Initialize selabel
 * @param ts		transaction set
 * @param path		path to contexts file
 * @return		RPMRC_OK on success, RPMRC_FAIL otherwise
 */
rpmRC rpmtsSELabelInit(rpmts ts, const char * path);

/** \ingroup rpmts
 * Clean up selabel
 * @param ts		transaction set
 */
void rpmtsSELabelFini(rpmts ts);

#ifdef __cplusplus
}
#endif
#endif /* _RPMTS_INTERNAL_H */
