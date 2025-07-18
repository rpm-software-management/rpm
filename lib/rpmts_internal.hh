#ifndef _RPMTS_INTERNAL_H
#define _RPMTS_INTERNAL_H

#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

#include <rpm/rpmts.h>
#include <rpm/rpmstrpool.h>

#include "rpmal.hh"		/* XXX availablePackage */
#include "fprint.hh"
#include "keystore.hh"
#include "rpmlock.hh"
#include "rpmdb_internal.hh"
#include "rpmscript.hh"
#include "rpmtriggers.hh"

struct diskspaceInfo {
    std::string mntPoint;/*!< File system mount point */
    dev_t dev;		/*!< File system device number. */
    int64_t bneeded;	/*!< No. of blocks needed. */
    int64_t ineeded;	/*!< No. of inodes needed. */
    int64_t bsize;	/*!< File system block size. */
    int64_t bavail;	/*!< No. of blocks available. */
    int64_t iavail;	/*!< No. of inodes available. */
    int64_t obneeded;	/*!< Bookkeeping to avoid duplicate reports */
    int64_t oineeded;	/*!< Bookkeeping to avoid duplicate reports */
    int64_t bdelta;	/*!< Delta for temporary space need on updates */
    int64_t idelta;	/*!< Delta for temporary inode need on updates */

    int rotational;	/*!< Rotational media? */
};

/* Transaction set elements information */
typedef struct tsMembers_s {
    rpmstrPool pool;		/*!< Global string pool */
    packageHash removedPackages;	/*!< Set of packages being removed. */
    packageHash installedPackages;	/*!< Set of installed packages */
    rpmal addedPackages;	/*!< Set of packages being installed. */

    rpmds rpmlib;		/*!< rpmlib() dependency set. */
    std::vector<rpmte> order;	/*!< Packages sorted by dependencies. */
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
    int notifyStyle;		/*!< Callback style (header vs rpmte) */

    rpmtsChangeFunction change;	/*!< Change callback function. */
    void *changeData;		/*!< Change callback private data. */

    rpmprobFilterFlags ignoreSet;
				/*!< Bits to filter current problems. */

    std::unordered_map<dev_t,diskspaceInfo> dsi;
				/*!< Per filesystem disk/inode usage. */

    rpmdb rdb;			/*!< Install database handle. */
    int dbmode;			/*!< Install database open mode. */

    tsMembers members;		/*!< Transaction set member info (order etc) */

    char * rootDir;		/*!< Path to top of install tree. */
    char * lockPath;		/*!< Transaction lock path */
    rpmlock lock;		/*!< Transaction lock file */
    FD_t scriptFd;		/*!< Scriptlet stdout/stderr. */
    rpm_tid_t tid;		/*!< Transaction id. */

    rpm_color_t color;		/*!< Transaction color bits. */
    rpm_color_t prefcolor;	/*!< Preferred file color. */

    rpmVSFlags vsflags;		/*!< Signature/digest verification flags. */
    rpmVSFlags vfyflags;	/*!< Package verification flags */
    int vfylevel;		/*!< Package verification level */
    rpmKeyring keyring;		/*!< Keyring in use. */
    rpm::keystore *keystore;	/*! <Keystore in use. */

    ARGV_t netsharedPaths;	/*!< From %{_netsharedpath} */
    ARGV_t installLangs;	/*!< From %{_install_langs} */

    struct rpmop_s ops[RPMTS_OP_MAX];

    rpmPlugins plugins;		/*!< Transaction plugins */

    std::atomic_int nrefs;	/*!< Reference count. */

    rpmtriggers trigs2run;   /*!< Transaction file triggers */

    int min_writes;             /*!< macro minimize_writes used */

    time_t overrideTime;	/*!< Time value used when overriding system clock. */
    int scriptError;		/*!< scriptlet error tracking */
};

/** \ingroup rpmts
 * Return transaction global string pool handle, creating the pool if needed.
 * @param ts		transaction set
 * @return		string pool handle (weak ref)
 */
RPM_GNUC_INTERNAL
rpmstrPool rpmtsPool(rpmts ts);

RPM_GNUC_INTERNAL
tsMembers rpmtsMembers(rpmts ts);

/* Return rpmdb iterator with removals optionally pruned out */
RPM_GNUC_INTERNAL
rpmdbMatchIterator rpmtsPrunedIterator(rpmts ts, rpmDbiTagVal tag,
					      const char * key, int prune);

/* Return rpmdb iterator locked to a single rpmte */
RPM_GNUC_INTERNAL
rpmdbMatchIterator rpmtsTeIterator(rpmts ts, rpmte te, int prune);

RPM_GNUC_INTERNAL
rpmal rpmtsCreateAl(rpmts ts, rpmElementTypes types);

/* returns -1 for retry, 0 for ignore and 1 for not found */
RPM_GNUC_INTERNAL
int rpmtsSolve(rpmts ts, rpmds key);

RPM_GNUC_INTERNAL
rpmRC rpmtsSetupTransactionPlugins(rpmts ts);

RPM_GNUC_INTERNAL
rpmRC runScript(rpmts ts, rpmte te, Header h, ARGV_const_t prefixes,
		       rpmScript script, int arg1, int arg2);


RPM_GNUC_INTERNAL
int rpmtsNotifyChange(rpmts ts, int event, rpmte te, rpmte other);

RPM_GNUC_INTERNAL
rpm_time_t rpmtsGetTime(rpmts ts, time_t step);

RPM_GNUC_INTERNAL
rpmts rpmtxnTs(rpmtxn txn);

RPM_GNUC_INTERNAL
const char *rpmtxnRootDir(rpmtxn txn);
#endif /* _RPMTS_INTERNAL_H */
