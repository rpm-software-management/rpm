/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class LockStats {
    // no public constructor
    /* package */ LockStats() {}

    private int st_id;
    public int getId() {
        return st_id;
    }

    private int st_cur_maxid;
    public int getCurMaxId() {
        return st_cur_maxid;
    }

    private int st_maxlocks;
    public int getMaxLocks() {
        return st_maxlocks;
    }

    private int st_maxlockers;
    public int getMaxLockers() {
        return st_maxlockers;
    }

    private int st_maxobjects;
    public int getMaxObjects() {
        return st_maxobjects;
    }

    private int st_nmodes;
    public int getNumModes() {
        return st_nmodes;
    }

    private int st_nlockers;
    public int getNumLockers() {
        return st_nlockers;
    }

    private int st_nlocks;
    public int getNumLocks() {
        return st_nlocks;
    }

    private int st_maxnlocks;
    public int getMaxNlocks() {
        return st_maxnlocks;
    }

    private int st_maxnlockers;
    public int getMaxNlockers() {
        return st_maxnlockers;
    }

    private int st_nobjects;
    public int getNobjects() {
        return st_nobjects;
    }

    private int st_maxnobjects;
    public int getMaxNobjects() {
        return st_maxnobjects;
    }

    private int st_nrequests;
    public int getNumRequests() {
        return st_nrequests;
    }

    private int st_nreleases;
    public int getNumReleases() {
        return st_nreleases;
    }

    private int st_nupgrade;
    public int getNumUpgrade() {
        return st_nupgrade;
    }

    private int st_ndowngrade;
    public int getNumDowngrade() {
        return st_ndowngrade;
    }

    private int st_lock_wait;
    public int getLockWait() {
        return st_lock_wait;
    }

    private int st_lock_nowait;
    public int getLockNowait() {
        return st_lock_nowait;
    }

    private int st_ndeadlocks;
    public int getNumDeadlocks() {
        return st_ndeadlocks;
    }

    private int st_locktimeout;
    public int getLockTimeout() {
        return st_locktimeout;
    }

    private int st_nlocktimeouts;
    public int getNumLockTimeouts() {
        return st_nlocktimeouts;
    }

    private int st_txntimeout;
    public int getTxnTimeout() {
        return st_txntimeout;
    }

    private int st_ntxntimeouts;
    public int getNumTxnTimeouts() {
        return st_ntxntimeouts;
    }

    private int st_objs_wait;
    public int getObjsWait() {
        return st_objs_wait;
    }

    private int st_objs_nowait;
    public int getObjsNowait() {
        return st_objs_nowait;
    }

    private int st_lockers_wait;
    public int getLockersWait() {
        return st_lockers_wait;
    }

    private int st_lockers_nowait;
    public int getLockersNowait() {
        return st_lockers_nowait;
    }

    private int st_locks_wait;
    public int getLocksWait() {
        return st_locks_wait;
    }

    private int st_locks_nowait;
    public int getLocksNowait() {
        return st_locks_nowait;
    }

    private int st_region_wait;
    public int getRegionWait() {
        return st_region_wait;
    }

    private int st_region_nowait;
    public int getRegionNowait() {
        return st_region_nowait;
    }

    private int st_hash_len;
    public int getHashLen() {
        return st_hash_len;
    }

    private int st_regsize;
    public int getRegSize() {
        return st_regsize;
    }

    public String toString() {
        return "LockStats:"
            + "\n  st_id=" + st_id
            + "\n  st_cur_maxid=" + st_cur_maxid
            + "\n  st_maxlocks=" + st_maxlocks
            + "\n  st_maxlockers=" + st_maxlockers
            + "\n  st_maxobjects=" + st_maxobjects
            + "\n  st_nmodes=" + st_nmodes
            + "\n  st_nlockers=" + st_nlockers
            + "\n  st_nlocks=" + st_nlocks
            + "\n  st_maxnlocks=" + st_maxnlocks
            + "\n  st_maxnlockers=" + st_maxnlockers
            + "\n  st_nobjects=" + st_nobjects
            + "\n  st_maxnobjects=" + st_maxnobjects
            + "\n  st_nrequests=" + st_nrequests
            + "\n  st_nreleases=" + st_nreleases
            + "\n  st_nupgrade=" + st_nupgrade
            + "\n  st_ndowngrade=" + st_ndowngrade
            + "\n  st_lock_wait=" + st_lock_wait
            + "\n  st_lock_nowait=" + st_lock_nowait
            + "\n  st_ndeadlocks=" + st_ndeadlocks
            + "\n  st_locktimeout=" + st_locktimeout
            + "\n  st_nlocktimeouts=" + st_nlocktimeouts
            + "\n  st_txntimeout=" + st_txntimeout
            + "\n  st_ntxntimeouts=" + st_ntxntimeouts
            + "\n  st_objs_wait=" + st_objs_wait
            + "\n  st_objs_nowait=" + st_objs_nowait
            + "\n  st_lockers_wait=" + st_lockers_wait
            + "\n  st_lockers_nowait=" + st_lockers_nowait
            + "\n  st_locks_wait=" + st_locks_wait
            + "\n  st_locks_nowait=" + st_locks_nowait
            + "\n  st_region_wait=" + st_region_wait
            + "\n  st_region_nowait=" + st_region_nowait
            + "\n  st_hash_len=" + st_hash_len
            + "\n  st_regsize=" + st_regsize
            ;
    }
}
