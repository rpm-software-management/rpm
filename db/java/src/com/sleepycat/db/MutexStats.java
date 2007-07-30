/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class MutexStats {
    // no public constructor
    /* package */ MutexStats() {}

    private int st_mutex_align;
    public int getMutexAlign() {
        return st_mutex_align;
    }

    private int st_mutex_tas_spins;
    public int getMutexTasSpins() {
        return st_mutex_tas_spins;
    }

    private int st_mutex_cnt;
    public int getMutexCount() {
        return st_mutex_cnt;
    }

    private int st_mutex_free;
    public int getMutexFree() {
        return st_mutex_free;
    }

    private int st_mutex_inuse;
    public int getMutexInuse() {
        return st_mutex_inuse;
    }

    private int st_mutex_inuse_max;
    public int getMutexInuseMax() {
        return st_mutex_inuse_max;
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
        return "MutexStats:"
            + "\n  st_mutex_align=" + st_mutex_align
            + "\n  st_mutex_tas_spins=" + st_mutex_tas_spins
            + "\n  st_mutex_cnt=" + st_mutex_cnt
            + "\n  st_mutex_free=" + st_mutex_free
            + "\n  st_mutex_inuse=" + st_mutex_inuse
            + "\n  st_mutex_inuse_max=" + st_mutex_inuse_max
            + "\n  st_region_wait=" + st_region_wait
            + "\n  st_region_nowait=" + st_region_nowait
            + "\n  st_regsize=" + st_regsize
            ;
    }
}
