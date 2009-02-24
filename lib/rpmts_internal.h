#ifndef _RPMTS_INTERNAL_H
#define _RPMTS_INTERNAL_H

#include <rpm/rpmts.h>
#include <rpm/rpmal.h>	/* XXX availablePackage */

#include "lib/rpmhash.h"	/* XXX hashTable */
#include "lib/fprint.h"

/** \ingroup rpmts
 */
typedef	struct diskspaceInfo_s * rpmDiskSpaceInfo;

/** \ingroup rpmts
 */
struct diskspaceInfo_s {
    dev_t dev;		/*!< File system device number. */
    int64_t bneeded;	/*!< No. of blocks needed. */
    int64_t ineeded;	/*!< No. of inodes needed. */
    int64_t bsize;	/*!< File system block size. */
    int64_t bavail;	/*!< No. of blocks available. */
    int64_t iavail;	/*!< No. of inodes available. */
};

/** \ingroup rpmts
 * Adjust for root only reserved space. On linux e2fs, this is 5%.
 */
#define	adj_fs_blocks(_nb)	(((_nb) * 21) / 20)

/* argon thought a shift optimization here was a waste of time...  he's
   probably right :-( */
#define BLOCK_ROUND(size, block) (((size) + (block) - 1) / (block))


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

    rpmps probs;		/*!< Current problems in transaction. */
    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    unsigned int filesystemCount;	/*!< No. of mounted filesystems. */
    const char ** filesystems;	/*!< Mounted filesystem names. */
    rpmDiskSpaceInfo dsi;	/*!< Per filesystem disk/inode usage. */
    uint64_t fileCount;		/*!< Total number of files in transaction */

    rpmdb rdb;			/*!< Install database handle. */
    int dbmode;			/*!< Install database open mode. */

    int * removedPackages;	/*!< Set of packages being removed. */
    int numRemovedPackages;	/*!< No. removed package instances. */
    int allocedRemovedPackages;	/*!< Size of removed packages array. */

    rpmal addedPackages;	/*!< Set of packages being installed. */
    int numAddedPackages;	/*!< No. added package instances. */

    rpmte * order;		/*!< Packages sorted by dependencies. */
    int orderCount;		/*!< No. of transaction elements. */
    int orderAlloced;		/*!< No. of allocated transaction elements. */
    int unorderedSuccessors;	/*!< Index of 1st element of successors. */
    int ntrees;			/*!< No. of dependency trees. */
    int maxDepth;		/*!< Maximum depth of dependency tree(s). */

    int selinuxEnabled;		/*!< Is SE linux enabled? */
    int chrootDone;		/*!< Has chroot(2) been been done? */
    char * rootDir;		/*!< Path to top of install tree. */
    char * currDir;		/*!< Current working directory. */
    FD_t scriptFd;		/*!< Scriptlet stdout/stderr. */
    int delta;			/*!< Delta for reallocation. */
    rpm_tid_t tid;		/*!< Transaction id. */

    rpm_color_t color;		/*!< Transaction color bits. */
    rpm_color_t prefcolor;	/*!< Preferred file color. */

    rpmVSFlags vsflags;		/*!< Signature/digest verification flags. */

    rpmKeyring keyring;		/*!< Keyring in use. */

    ARGV_t netsharedPaths;	/*!< From %{_netsharedpath} */
    ARGV_t installLangs;	/*!< From %{_install_langs} */

    struct rpmop_s ops[RPMTS_OP_MAX];

    rpmSpec spec;		/*!< Spec file control structure. */

    int nrefs;			/*!< Reference count. */
};

#endif /* _RPMTS_INTERNAL_H */
