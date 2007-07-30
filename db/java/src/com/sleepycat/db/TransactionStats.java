/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.DbUtil;

public class TransactionStats
{
    // no public constructor
    /* package */ TransactionStats() {}

    public static class Active {        // no public constructor
        /* package */ Active() {}

        private int txnid;
        public int getTxnId() {
            return txnid;
        }

        private int parentid;
        public int getParentId() {
            return parentid;
        }

        private int pid;
        public int getPid() {
            return pid;
        }

        private LogSequenceNumber lsn;
        public LogSequenceNumber getLsn() {
            return lsn;
        }

        private LogSequenceNumber read_lsn;
        public LogSequenceNumber getReadLsn() {
            return read_lsn;
        }

        private int mvcc_ref;
        public int getMultiversionRef() {
            return mvcc_ref;
        }

        private int status;
        public int getStatus() {
            return status;
        }

        private int xa_status;
        public int getXaStatus() {
            return xa_status;
        }

        private byte[] xid;
        public byte[] getXId() {
            return xid;
        }

        private String name;
        public String getName() {
            return name;
        }

        public String toString() {
            return "Active:"
                + "\n      txnid=" + txnid
                + "\n      parentid=" + parentid
                + "\n      pid=" + pid
                + "\n      lsn=" + lsn
                + "\n      read_lsn=" + read_lsn
                + "\n      mvcc_ref=" + mvcc_ref
                + "\n      status=" + status
                + "\n      xa_status=" + xa_status
                + "\n      xid=" + DbUtil.byteArrayToString(xid)
                + "\n      name=" + name
                ;
        }
    };

    private int st_nrestores;
    public int getNumRestores() {
        return st_nrestores;
    }

    private LogSequenceNumber st_last_ckp;
    public LogSequenceNumber getLastCkp() {
        return st_last_ckp;
    }

    private long st_time_ckp;
    public long getTimeCkp() {
        return st_time_ckp;
    }

    private int st_last_txnid;
    public int getLastTxnId() {
        return st_last_txnid;
    }

    private int st_maxtxns;
    public int getMaxTxns() {
        return st_maxtxns;
    }

    private int st_naborts;
    public int getNaborts() {
        return st_naborts;
    }

    private int st_nbegins;
    public int getNumBegins() {
        return st_nbegins;
    }

    private int st_ncommits;
    public int getNumCommits() {
        return st_ncommits;
    }

    private int st_nactive;
    public int getNactive() {
        return st_nactive;
    }

    private int st_nsnapshot;
    public int getNumSnapshot() {
        return st_nsnapshot;
    }

    private int st_maxnactive;
    public int getMaxNactive() {
        return st_maxnactive;
    }

    private int st_maxnsnapshot;
    public int getMaxNsnapshot() {
        return st_maxnsnapshot;
    }

    private Active[] st_txnarray;
    public Active[] getTxnarray() {
        return st_txnarray;
    }

    private int st_region_wait;
    public int getRegionWait() {
        return st_region_wait;
    }

    private int st_region_nowait;
    public int getRegionNowait() {
        return st_region_nowait;
    }

    private int st_regsize;
    public int getRegSize() {
        return st_regsize;
    }

    public String toString() {
        return "TransactionStats:"
            + "\n  st_nrestores=" + st_nrestores
            + "\n  st_last_ckp=" + st_last_ckp
            + "\n  st_time_ckp=" + st_time_ckp
            + "\n  st_last_txnid=" + st_last_txnid
            + "\n  st_maxtxns=" + st_maxtxns
            + "\n  st_naborts=" + st_naborts
            + "\n  st_nbegins=" + st_nbegins
            + "\n  st_ncommits=" + st_ncommits
            + "\n  st_nactive=" + st_nactive
            + "\n  st_nsnapshot=" + st_nsnapshot
            + "\n  st_maxnactive=" + st_maxnactive
            + "\n  st_maxnsnapshot=" + st_maxnsnapshot
            + "\n  st_txnarray=" + DbUtil.objectArrayToString(st_txnarray, "st_txnarray")
            + "\n  st_region_wait=" + st_region_wait
            + "\n  st_region_nowait=" + st_region_nowait
            + "\n  st_regsize=" + st_regsize
            ;
    }
}
// end of TransactionStats.java
