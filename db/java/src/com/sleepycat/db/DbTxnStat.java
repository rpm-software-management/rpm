/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbTxnStat object is used to return transaction subsystem
 *  statistics.</p>
 */
public class DbTxnStat {
    public static class Active {
        /**
         *  The transaction ID of the transaction.
         *</ul>
         *
         */
        public int txnid;
        /**
         *  The transaction ID of the parent transaction (or 0, if no
         *  parent).
         *</ul>
         *
         */
        public int parentid;
        /**
         *  The current log sequence number when the transaction was
         *  begun.
         *</ul>
         *
         */
        public DbLsn lsn;
        /**
         *  If the transaction is an XA transaction, the status of the
         *  transaction, otherwise 0.
         *</ul>
         *
         */
        public int xa_status;
        /**
         *  If the transaction is an XA transaction, the transaction's
         *  XA ID.
         *</ul>
         *
         */
        public byte[] xid;


        /**
         *  Provide a string representation of all the fields
         *  contained within this class.
         *
         * @return    The string representation.
         */
        public String toString() {
            return "Active:"
                    + "\n      txnid=" + txnid
                    + "\n      parentid=" + parentid
                    + "\n      lsn=" + lsn
                    + "\n      xa_status=" + xa_status
                    + "\n      xid=" + DbUtil.byteArrayToString(xid)
                    ;
        }
    }


    /**
     *  The LSN of the last checkpoint.
     *</ul>
     *
     */
    public DbLsn st_last_ckp;
    /**
     *  The time the last completed checkpoint finished (as the number
     *  of seconds since the Epoch, returned by the IEEE/ANSI Std
     *  1003.1 (POSIX) <b>time</b> function).
     *</ul>
     *
     */
    public long st_time_ckp;
    /**
     *  The last transaction ID allocated.
     *</ul>
     *
     */
    public int st_last_txnid;
    /**
     *  The maximum number of active transactions configured.
     *</ul>
     *
     */
    public int st_maxtxns;
    /**
     *  The number of transactions that have aborted.
     *</ul>
     *
     */
    public int st_naborts;
    /**
     *  The number of transactions that have begun.
     *</ul>
     *
     */
    public int st_nbegins;
    /**
     *  The number of transactions that have committed.
     *</ul>
     *
     */
    public int st_ncommits;
    /**
     *  The number of transactions that are currently active.
     *</ul>
     *
     */
    public int st_nactive;
    /**
     *  The number of transactions that have been restored.
     *</ul>
     *
     */
    public int st_nrestores;
    /**
     *  The maximum number of active transactions at any one time.
     *
     *</ul>
     *
     */
    public int st_maxnactive;
    public Active st_txnarray[];
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
     *  The size of the region.
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
        return "DbTxnStat:"
                + "\n  st_last_ckp=" + st_last_ckp
                + "\n  st_time_ckp=" + st_time_ckp
                + "\n  st_last_txnid=" + st_last_txnid
                + "\n  st_maxtxns=" + st_maxtxns
                + "\n  st_naborts=" + st_naborts
                + "\n  st_nbegins=" + st_nbegins
                + "\n  st_ncommits=" + st_ncommits
                + "\n  st_nactive=" + st_nactive
                + "\n  st_nrestores=" + st_nrestores
                + "\n  st_maxnactive=" + st_maxnactive
                + "\n  st_txnarray=" + DbUtil.objectArrayToString(st_txnarray, "st_txnarray")
                + "\n  st_region_wait=" + st_region_wait
                + "\n  st_region_nowait=" + st_region_nowait
                + "\n  st_regsize=" + st_regsize
                ;
    }
}
// end of DbTxnStat.java
