/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbLockStat object is used to return lock region statistics.
 *  </p>
 */
public class DbLockStat {
    /**
     *  The last allocated locker ID.
     *</ul>
     *
     */
    public int st_id;
    /**
     *  The current maximum unused locker ID.
     *</ul>
     *
     */
    public int st_cur_maxid;
    /**
     *  The maximum number of locks possible.
     *</ul>
     *
     */
    public int st_maxlocks;
    /**
     *  The maximum number of lockers possible.
     *</ul>
     *
     */
    public int st_maxlockers;
    /**
     *  The maximum number of lock objects possible.
     *</ul>
     *
     */
    public int st_maxobjects;
    /**
     *  The number of lock modes.
     *</ul>
     *
     */
    public int st_nmodes;
    /**
     *  The number of current locks.
     *</ul>
     *
     */
    public int st_nlocks;
    /**
     *  The maximum number of locks at any one time.
     *</ul>
     *
     */
    public int st_maxnlocks;
    /**
     *  The number of current lockers.
     *</ul>
     *
     */
    public int st_nlockers;
    /**
     *  The maximum number of lockers at any one time.
     *</ul>
     *
     */
    public int st_maxnlockers;
    /**
     *  The number of current lock objects.
     *</ul>
     *
     */
    public int st_nobjects;
    /**
     *  The maximum number of lock objects at any one time.
     *</ul>
     *
     */
    public int st_maxnobjects;
    /**
     *  The total number of locks not immediately available due to
     *  conflicts.
     *</ul>
     *
     */
    public int st_nconflicts;
    /**
     *  The total number of locks requested.
     *</ul>
     *
     */
    public int st_nrequests;
    /**
     *  The total number of locks released.
     *</ul>
     *
     */
    public int st_nreleases;
    /**
     *  The total number of lock requests failing because {@link
     *  com.sleepycat.db.Db#DB_LOCK_NOWAIT Db.DB_LOCK_NOWAIT} was set.
     *
     *</ul>
     *
     */
    public int st_nnowaits;
    /**
     *  The number of deadlocks.
     *</ul>
     *
     */
    public int st_ndeadlocks;
    /**
     *  Lock timeout value.
     *</ul>
     *
     */
    public int st_locktimeout;
    /**
     *  The number of lock requests that have timed out.
     *</ul>
     *
     */
    public int st_nlocktimeouts;
    /**
     *  Transaction timeout value.
     *</ul>
     *
     */
    public int st_txntimeout;
    /**
     *  The number of transactions that have timed out. This value is
     *  also a component of <b>st_ndeadlocks</b> , the total number of
     *  deadlocks detected.
     *</ul>
     *
     */
    public int st_ntxntimeouts;
    /**
     *  The number of times that a thread of control was forced to
     *  wait before obtaining the region lock.
     *</ul>
     *
     */
    public int st_region_wait;
    /**
     *  The number of times that a thread of control was able to
     *  obtain the region lock without waiting.
     *</ul>
     *
     */
    public int st_region_nowait;
    /**
     *  The size of the lock region.
     *</ul>
     *
     */
    public int st_regsize;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbLockStat:"
                + "\n  st_id=" + st_id
                + "\n  st_cur_maxid=" + st_cur_maxid
                + "\n  st_maxlocks=" + st_maxlocks
                + "\n  st_maxlockers=" + st_maxlockers
                + "\n  st_maxobjects=" + st_maxobjects
                + "\n  st_nmodes=" + st_nmodes
                + "\n  st_nlocks=" + st_nlocks
                + "\n  st_maxnlocks=" + st_maxnlocks
                + "\n  st_nlockers=" + st_nlockers
                + "\n  st_maxnlockers=" + st_maxnlockers
                + "\n  st_nobjects=" + st_nobjects
                + "\n  st_maxnobjects=" + st_maxnobjects
                + "\n  st_nconflicts=" + st_nconflicts
                + "\n  st_nrequests=" + st_nrequests
                + "\n  st_nreleases=" + st_nreleases
                + "\n  st_nnowaits=" + st_nnowaits
                + "\n  st_ndeadlocks=" + st_ndeadlocks
                + "\n  st_locktimeout=" + st_locktimeout
                + "\n  st_nlocktimeouts=" + st_nlocktimeouts
                + "\n  st_txntimeout=" + st_txntimeout
                + "\n  st_ntxntimeouts=" + st_ntxntimeouts
                + "\n  st_region_wait=" + st_region_wait
                + "\n  st_region_nowait=" + st_region_nowait
                + "\n  st_regsize=" + st_regsize
                ;
    }
}
// end of DbLockStat.java
