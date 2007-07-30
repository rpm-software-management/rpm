/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class LogStats {
    // no public constructor
    /* package */ LogStats() {}

    private int st_magic;
    public int getMagic() {
        return st_magic;
    }

    private int st_version;
    public int getVersion() {
        return st_version;
    }

    private int st_mode;
    public int getMode() {
        return st_mode;
    }

    private int st_lg_bsize;
    public int getLgBSize() {
        return st_lg_bsize;
    }

    private int st_lg_size;
    public int getLgSize() {
        return st_lg_size;
    }

    private int st_wc_bytes;
    public int getWcBytes() {
        return st_wc_bytes;
    }

    private int st_wc_mbytes;
    public int getWcMbytes() {
        return st_wc_mbytes;
    }

    private int st_record;
    public int getRecord() {
        return st_record;
    }

    private int st_w_bytes;
    public int getWBytes() {
        return st_w_bytes;
    }

    private int st_w_mbytes;
    public int getWMbytes() {
        return st_w_mbytes;
    }

    private int st_wcount;
    public int getWCount() {
        return st_wcount;
    }

    private int st_wcount_fill;
    public int getWCountFill() {
        return st_wcount_fill;
    }

    private int st_rcount;
    public int getRCount() {
        return st_rcount;
    }

    private int st_scount;
    public int getSCount() {
        return st_scount;
    }

    private int st_region_wait;
    public int getRegionWait() {
        return st_region_wait;
    }

    private int st_region_nowait;
    public int getRegionNowait() {
        return st_region_nowait;
    }

    private int st_cur_file;
    public int getCurFile() {
        return st_cur_file;
    }

    private int st_cur_offset;
    public int getCurOffset() {
        return st_cur_offset;
    }

    private int st_disk_file;
    public int getDiskFile() {
        return st_disk_file;
    }

    private int st_disk_offset;
    public int getDiskOffset() {
        return st_disk_offset;
    }

    private int st_maxcommitperflush;
    public int getMaxCommitperflush() {
        return st_maxcommitperflush;
    }

    private int st_mincommitperflush;
    public int getMinCommitperflush() {
        return st_mincommitperflush;
    }

    private int st_regsize;
    public int getRegSize() {
        return st_regsize;
    }

    public String toString() {
        return "LogStats:"
            + "\n  st_magic=" + st_magic
            + "\n  st_version=" + st_version
            + "\n  st_mode=" + st_mode
            + "\n  st_lg_bsize=" + st_lg_bsize
            + "\n  st_lg_size=" + st_lg_size
            + "\n  st_wc_bytes=" + st_wc_bytes
            + "\n  st_wc_mbytes=" + st_wc_mbytes
            + "\n  st_record=" + st_record
            + "\n  st_w_bytes=" + st_w_bytes
            + "\n  st_w_mbytes=" + st_w_mbytes
            + "\n  st_wcount=" + st_wcount
            + "\n  st_wcount_fill=" + st_wcount_fill
            + "\n  st_rcount=" + st_rcount
            + "\n  st_scount=" + st_scount
            + "\n  st_region_wait=" + st_region_wait
            + "\n  st_region_nowait=" + st_region_nowait
            + "\n  st_cur_file=" + st_cur_file
            + "\n  st_cur_offset=" + st_cur_offset
            + "\n  st_disk_file=" + st_disk_file
            + "\n  st_disk_offset=" + st_disk_offset
            + "\n  st_maxcommitperflush=" + st_maxcommitperflush
            + "\n  st_mincommitperflush=" + st_mincommitperflush
            + "\n  st_regsize=" + st_regsize
            ;
    }
}
